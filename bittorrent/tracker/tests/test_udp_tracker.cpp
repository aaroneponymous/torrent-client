#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <array>
#include <mutex>

#include "../include/udp_tracker.hpp"
#include "../include/types.hpp"

#if defined(_WIN32)
  #error "UDP tests are POSIX-only in this file (sockets API). Add Winsock variant if needed."
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

using namespace bittorrent::tracker;
using namespace std::chrono_literals;

// --------------------- test-side byte helpers ---------------------
static inline void t_put_u32(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    b.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    b.push_back(static_cast<uint8_t>((v >>  8) & 0xFF));
    b.push_back(static_cast<uint8_t>( v        & 0xFF));
}
static inline void t_put_u64(std::vector<uint8_t>& b, uint64_t v) {
    t_put_u32(b, static_cast<uint32_t>(v >> 32));
    t_put_u32(b, static_cast<uint32_t>(v & 0xFFFFFFFFu));
}
static inline uint32_t t_get_u32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}
static inline uint64_t t_get_u64(const uint8_t* p) {
    return (uint64_t(t_get_u32(p)) << 32) | t_get_u32(p + 4);
}

// --------------------- Fake UDP tracker server ---------------------
class FakeUdpTrackerServer {
public:
    FakeUdpTrackerServer()
      : running_(false), sock_(-1), port_(0),
        connId_(0x0123456789ABCDEFULL),
        sendErrorOnAnnounce_(false), errorMsg_("nope"),
        sendErrorOnScrape_(false), errorScrapeMsg_("scrape nope"),
        onceShortAnnounce_(false), onceActionMismatch_(false), onceWrongTx_(false),
        connectCount_(0), announceCount_(0), scrapeCount_(0) {}

    ~FakeUdpTrackerServer() { stop(); }

    // Set peers returned by announce (IPv4 strings)
    void setPeers(std::vector<std::pair<std::string,int>> peers) {
        std::lock_guard<std::mutex> lk(mu_);
        peers_ = std::move(peers);
    }

    // Configure scrape stats to return (seeders, completed, leechers)
    void setScrapeTriplet(uint32_t s, uint32_t c, uint32_t l) {
        std::lock_guard<std::mutex> lk(mu_);
        scrapeSeeders_ = s; scrapeCompleted_ = c; scrapeLeechers_ = l;
    }

    void setErrorOnAnnounce(bool on, std::string msg = "nope") {
        sendErrorOnAnnounce_ = on; errorMsg_ = std::move(msg);
    }
    void setErrorOnScrape(bool on, std::string msg = "nope") {
        sendErrorOnScrape_ = on; errorScrapeMsg_ = std::move(msg);
    }

    // “once” behaviors to exercise client retry paths
    void setShortAnnounceOnce(bool on = true) { onceShortAnnounce_ = on; }
    void setActionMismatchOnce(bool on = true) { onceActionMismatch_ = on; }
    void setWrongTxOnce(bool on = true) { onceWrongTx_ = on; }

    bool start() {
        if (running_.load()) return true;

        sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_ < 0) return false;

        int on = 1;
        ::setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

        // Set a short receive timeout so stop() is responsive
        timeval tv{}; tv.tv_sec = 0; tv.tv_usec = 200'000; // 200ms
        ::setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        sockaddr_in addr{}; addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1
        addr.sin_port = htons(0); // ephemeral

        if (::bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            ::close(sock_); sock_ = -1; return false;
        }

        sockaddr_in bound{}; socklen_t blen = sizeof(bound);
        if (::getsockname(sock_, reinterpret_cast<sockaddr*>(&bound), &blen) == 0) {
            port_ = ntohs(bound.sin_port);
        } else {
            ::close(sock_); sock_ = -1; return false;
        }

        running_.store(true);
        th_ = std::thread([this]{ this->loop(); });
        return true;
    }

    void stop() {
        if (!running_.exchange(false)) return;
        if (th_.joinable()) th_.join();
        if (sock_ >= 0) {
            ::close(sock_);
            sock_ = -1;
        }
    }

    uint16_t port() const { return port_; }

    // stats
    int connectCount() const { return connectCount_.load(); }
    int announceCount() const { return announceCount_.load(); }
    int scrapeCount() const { return scrapeCount_.load(); }

private:
    void loop() {
        while (running_.load()) {
            uint8_t buf[65536];
            sockaddr_storage cli{};
            socklen_t clen = sizeof(cli);

            const ssize_t n = ::recvfrom(sock_, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&cli), &clen);
            if (n <= 0) continue;

            // Determine action (connect has protocol id first; action at offset 8)
            if (n < 12) continue; // malformed

            const uint32_t action = t_get_u32(buf + 8);
            const uint32_t tx     = t_get_u32(buf + 12);

            switch (action) {
                case 0: // connect
                    connectCount_.fetch_add(1);
                    send_connect(cli, clen, tx);
                    break;
                case 1: // announce
                    announceCount_.fetch_add(1);
                    if (sendErrorOnAnnounce_) {
                        send_error(cli, clen, tx, errorMsg_);
                    } else if (onceShortAnnounce_) {
                        onceShortAnnounce_ = false;
                        send_announce_truncated(cli, clen, tx);
                    } else if (onceActionMismatch_) {
                        onceActionMismatch_ = false;
                        send_announce_action_mismatch(cli, clen, tx);
                    } else if (onceWrongTx_) {
                        onceWrongTx_ = false;
                        send_announce_wrong_tx(cli, clen, tx);
                    } else {
                        send_announce(cli, clen, tx);
                    }
                    break;
                case 2: // scrape
                    scrapeCount_.fetch_add(1);
                    if (sendErrorOnScrape_) {
                        send_error(cli, clen, tx, errorScrapeMsg_);
                    } else {
                        send_scrape(cli, clen, tx, n);
                    }
                    break;
                default:
                    send_error(cli, clen, tx, "unsupported action");
                    break;
            }
        }
    }

    void send_connect(const sockaddr_storage& cli, socklen_t clen, uint32_t tx) {
        // Response: action(0), tx, connection_id(64)
        std::vector<uint8_t> out;
        t_put_u32(out, 0u);
        t_put_u32(out, tx);
        t_put_u64(out, connId_);
        ::sendto(sock_, out.data(), out.size(), 0, reinterpret_cast<const sockaddr*>(&cli), clen);
    }

    void send_announce_truncated(const sockaddr_storage& cli, socklen_t clen, uint32_t tx) {
        // Only 12 bytes (action + tx), deliberately shorter than 20 required bytes.
        std::vector<uint8_t> out;
        t_put_u32(out, 1u);      // action=announce
        t_put_u32(out, tx);      // same tx
        // no body
        ::sendto(sock_, out.data(), out.size(), 0, reinterpret_cast<const sockaddr*>(&cli), clen);
    }

    void send_announce_action_mismatch(const sockaddr_storage& cli, socklen_t clen, uint32_t tx) {
        // action=2 (scrape) but in response to announce; include 12 more bytes so n>=20
        std::vector<uint8_t> out;
        t_put_u32(out, 2u);      // wrong action
        t_put_u32(out, tx);
        t_put_u32(out, 0);
        t_put_u32(out, 0);
        t_put_u32(out, 0);
        ::sendto(sock_, out.data(), out.size(), 0, reinterpret_cast<const sockaddr*>(&cli), clen);
    }

    void send_announce_wrong_tx(const sockaddr_storage& cli, socklen_t clen, uint32_t tx) {
        // Correct action but wrong tx; full announce body present (ignored by client).
        uint32_t interval = 900, leech = 5, seed = 3;
        std::vector<uint8_t> out;
        t_put_u32(out, 1u);
        t_put_u32(out, tx + 1u); // wrong transaction id
        t_put_u32(out, interval);
        t_put_u32(out, leech);
        t_put_u32(out, seed);
        // no peers needed; it's a mismatch anyway
        ::sendto(sock_, out.data(), out.size(), 0, reinterpret_cast<const sockaddr*>(&cli), clen);
    }

    void send_announce(const sockaddr_storage& cli, socklen_t clen, uint32_t tx) {
        // Response: action(1), tx, interval, leechers, seeders, peers...
        uint32_t interval = 900, leech = 5, seed = 3;

        std::vector<uint8_t> out;
        t_put_u32(out, 1u);
        t_put_u32(out, tx);
        t_put_u32(out, interval);
        t_put_u32(out, leech);
        t_put_u32(out, seed);

        std::vector<std::pair<std::string,int>> peersCopy;
        {
            std::lock_guard<std::mutex> lk(mu_);
            peersCopy = peers_;
        }
        for (auto& [ip, port] : peersCopy) {
            in_addr ina{};
            ::inet_pton(AF_INET, ip.c_str(), &ina);
            const uint32_t ipn = ntohl(ina.s_addr); // convert to host order for packing
            t_put_u32(out, ipn);
            out.push_back(static_cast<uint8_t>((port >> 8) & 0xFF));
            out.push_back(static_cast<uint8_t>(port & 0xFF));
        }

        ::sendto(sock_, out.data(), out.size(), 0, reinterpret_cast<const sockaddr*>(&cli), clen);
    }

    void send_scrape(const sockaddr_storage& cli, socklen_t clen, uint32_t tx, ssize_t n) {
        // Request body after header is 20*N info_hashes.
        // Response: action(2), tx, then for each hash: seeders, completed, leechers.
        (void)n;
        uint32_t seeders, completed, leechers;
        {
            std::lock_guard<std::mutex> lk(mu_);
            seeders = scrapeSeeders_; completed = scrapeCompleted_; leechers = scrapeLeechers_;
        }

        std::vector<uint8_t> out;
        t_put_u32(out, 2u);
        t_put_u32(out, tx);

        int N = 1;
        if (n >= 16) {
            const ssize_t body = n - 16;
            if (body >= 0) N = static_cast<int>(body / 20);
        }
        if (N <= 0) N = 1;

        for (int i = 0; i < N; ++i) {
            t_put_u32(out, seeders);
            t_put_u32(out, completed);
            t_put_u32(out, leechers);
        }

        ::sendto(sock_, out.data(), out.size(), 0, reinterpret_cast<const sockaddr*>(&cli), clen);
    }

    void send_error(const sockaddr_storage& cli, socklen_t clen, uint32_t tx, const std::string& msg) {
        std::vector<uint8_t> out;
        t_put_u32(out, 3u);
        t_put_u32(out, tx);
        out.insert(out.end(), msg.begin(), msg.end());
        ::sendto(sock_, out.data(), out.size(), 0, reinterpret_cast<const sockaddr*>(&cli), clen);
    }

private:
    std::atomic<bool> running_;
    int sock_;
    uint16_t port_;
    uint64_t connId_;

    std::vector<std::pair<std::string,int>> peers_;
    uint32_t scrapeSeeders_{12}, scrapeCompleted_{34}, scrapeLeechers_{56};

    std::atomic<int> connectCount_;
    std::atomic<int> announceCount_;
    std::atomic<int> scrapeCount_;

    bool sendErrorOnAnnounce_;
    std::string errorMsg_;
    bool sendErrorOnScrape_;
    std::string errorScrapeMsg_;

    bool onceShortAnnounce_;
    bool onceActionMismatch_;
    bool onceWrongTx_;

    std::thread th_;
    std::mutex mu_;
};

// --------------------- helpers ---------------------
static InfoHash make_infohash(uint8_t seed = 0x00) {
    InfoHash ih{};
    for (size_t i = 0; i < ih.bytes.size(); ++i) ih.bytes[i] = static_cast<uint8_t>(seed + i);
    return ih;
}
static PeerID make_peerid(uint8_t seed = 0xA5) {
    PeerID p{};
    for (size_t i = 0; i < p.bytes.size(); ++i) p.bytes[i] = static_cast<uint8_t>(seed ^ i);
    return p;
}

// --------------------- TESTS ---------------------

TEST_CASE("UdpTracker: announce returns peers and stats (happy path)") {
    FakeUdpTrackerServer server;
    REQUIRE(server.start());
    server.setPeers({{"127.1.2.3", 6881}, {"10.0.0.2", 80}});

    UdpTracker udp;
    AnnounceRequest req{};
    req.infoHash = make_infohash(0x01);
    req.peerId   = make_peerid();
    req.port     = 51413;
    req.uploaded = 0;
    req.downloaded = 0;
    req.left = 42;
    req.event = AnnounceEvent::started;
    req.numwant = 10;
    req.key = 0xDEADBEEF;
    req.compact = true;
    req.no_peer_id = true;

    const std::string url = "udp://127.0.0.1:" + std::to_string(server.port()) + "/announce";

    auto res = udp.announce(req, url);
    REQUIRE(res.has_value());

    const AnnounceResponse& ar = res.get();
    CHECK(ar.interval   == 900);
    CHECK(ar.incomplete == 5);
    CHECK(ar.complete   == 3);

    REQUIRE(ar.peers.size() == 2);
    CHECK(ar.peers[0].ip == "127.1.2.3");
    CHECK(ar.peers[0].port == 6881);
    CHECK(ar.peers[1].ip == "10.0.0.2");
    CHECK(ar.peers[1].port == 80);

    // Ensure server saw a connect before/with announce
    CHECK(server.connectCount() >= 1);
    CHECK(server.announceCount() >= 1);

    server.stop();
}

TEST_CASE("UdpTracker: scrape returns stats for multiple info-hashes") {
    FakeUdpTrackerServer server;
    REQUIRE(server.start());
    server.setScrapeTriplet(/*seeders*/77, /*completed*/55, /*leechers*/99);

    UdpTracker udp;

    std::vector<InfoHash> hashes{ make_infohash(0x10), make_infohash(0x20) };
    const std::string url = "udp://127.0.0.1:" + std::to_string(server.port()) + "/scrape";

    auto res = udp.scrape(hashes, url);
    REQUIRE(res.has_value());

    const auto& m = res.get();
    REQUIRE(m.size() == hashes.size());

    for (const auto& h : hashes) {
        auto it = m.find(h);
        REQUIRE(it != m.end());
        CHECK(it->second.complete   == 77);
        CHECK(it->second.downloaded == 55);
        CHECK(it->second.incomplete == 99);
    }

    CHECK(server.connectCount() >= 1);
    CHECK(server.scrapeCount()  >= 1);

    server.stop();
}

TEST_CASE("UdpTracker: announce surfaces tracker error (action=3)") {
    FakeUdpTrackerServer server;
    REQUIRE(server.start());
    server.setErrorOnAnnounce(true, "nope nope nope");

    UdpTracker udp;
    AnnounceRequest req{};
    req.infoHash = make_infohash(0x02);
    req.peerId   = make_peerid(0xB4);
    req.port     = 51413;
    req.left     = 1;
    req.event    = AnnounceEvent::started;
    req.numwant  = 1;
    req.key      = 0xBADC0DE;

    const std::string url = "udp://127.0.0.1:" + std::to_string(server.port()) + "/announce";
    auto res = udp.announce(req, url);

    REQUIRE_FALSE(res.has_value());
    REQUIRE(res.error.has_value());
    CHECK(res.error->message.find("udp error") != std::string::npos);
    CHECK(res.error->message.find("nope nope nope") != std::string::npos);

    server.stop();
}

// --------------------- NEW TESTS ---------------------

TEST_CASE("UdpTracker: invalid URL scheme is rejected quickly") {
    UdpTracker udp;
    AnnounceRequest req{};
    req.infoHash = make_infohash(0x03);
    req.peerId   = make_peerid(0xC1);
    req.port     = 51413;
    req.left     = 10;
    req.event    = AnnounceEvent::started;
    req.numwant  = 5;
    req.key      = 1234;

    // Not a udp:// scheme
    const std::string url = "http://127.0.0.1:6969/announce";
    auto res = udp.announce(req, url);

    REQUIRE_FALSE(res.has_value());
    REQUIRE(res.error.has_value());
    CHECK(res.error->message.find("udp") != std::string::npos);
}

TEST_CASE("UdpTracker: empty scrape hashes returns empty map without network IO") {
    FakeUdpTrackerServer server;
    REQUIRE(server.start());

    UdpTracker udp;
    std::vector<InfoHash> hashes; // empty
    const std::string url = "udp://127.0.0.1:" + std::to_string(server.port()) + "/scrape";

    auto res = udp.scrape(hashes, url);
    REQUIRE(res.has_value());
    CHECK(res.get().empty());

    // Ensure server was not contacted for scrape (socket gets created but no packet sent)
    CHECK(server.scrapeCount() == 0);

    server.stop();
}

TEST_CASE("UdpTracker: truncated announce once -> client retries and succeeds") {
    FakeUdpTrackerServer server;
    REQUIRE(server.start());
    server.setPeers({{"127.0.0.9", 6881}});
    server.setShortAnnounceOnce(true); // send 12-byte response first

    UdpTracker udp;
    AnnounceRequest req{};
    req.infoHash = make_infohash(0x11);
    req.peerId   = make_peerid(0xA5);
    req.port     = 51413;
    req.left     = 5;
    req.event    = AnnounceEvent::started;
    req.numwant  = 1;
    req.key      = 42;

    const std::string url = "udp://127.0.0.1:" + std::to_string(server.port()) + "/announce";
    auto t0 = std::chrono::steady_clock::now();
    auto res = udp.announce(req, url);
    auto t1 = std::chrono::steady_clock::now();

    REQUIRE(res.has_value());
    CHECK(res.get().peers.size() == 1);
    CHECK(res.get().peers[0].ip == "127.0.0.9");

    // Expect at least one retry delay (~ >= 1s); keep this tolerant
    CHECK(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() >= 1000);

    server.stop();
}

TEST_CASE("UdpTracker: action mismatch once -> client retries and succeeds") {
    FakeUdpTrackerServer server;
    REQUIRE(server.start());
    server.setPeers({{"127.0.0.7", 51413}});
    server.setActionMismatchOnce(true); // first reply uses action=2 to an announce

    UdpTracker udp;
    AnnounceRequest req{};
    req.infoHash = make_infohash(0x22);
    req.peerId   = make_peerid(0xBB);
    req.port     = 51413;
    req.left     = 7;
    req.event    = AnnounceEvent::started;
    req.numwant  = 1;
    req.key      = 77;

    const std::string url = "udp://127.0.0.1:" + std::to_string(server.port()) + "/announce";
    auto t0 = std::chrono::steady_clock::now();
    auto res = udp.announce(req, url);
    auto t1 = std::chrono::steady_clock::now();

    REQUIRE(res.has_value());
    CHECK(res.get().peers.size() == 1);
    CHECK(res.get().peers[0].ip == "127.0.0.7");

    // Expect at least one retry delay (~ >= 1s)
    CHECK(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() >= 1000);

    server.stop();
}

TEST_CASE("UdpTracker: scrape error surfaces action=3 text") {
    FakeUdpTrackerServer server;
    REQUIRE(server.start());
    server.setErrorOnScrape(true, "scrape broken, sorry");

    UdpTracker udp;
    std::vector<InfoHash> hashes{ make_infohash(0x33) };
    const std::string url = "udp://127.0.0.1:" + std::to_string(server.port()) + "/scrape";

    auto res = udp.scrape(hashes, url);
    REQUIRE_FALSE(res.has_value());
    REQUIRE(res.error.has_value());
    CHECK(res.error->message.find("udp error") != std::string::npos);
    CHECK(res.error->message.find("scrape broken") != std::string::npos);

    server.stop();
}

TEST_CASE("UdpTracker: fresh instance causes a new connect (connId is not global)") {
    FakeUdpTrackerServer server;
    REQUIRE(server.start());
    server.setPeers({{"127.0.0.5", 7777}});

    const std::string url = "udp://127.0.0.1:" + std::to_string(server.port()) + "/announce";

    {
        UdpTracker udp1;
        AnnounceRequest req{};
        req.infoHash = make_infohash(0x44);
        req.peerId   = make_peerid(0x01);
        req.port     = 51413;
        req.left     = 1;
        req.event    = AnnounceEvent::started;
        req.numwant  = 1;
        req.key      = 1;
        auto r = udp1.announce(req, url);
        REQUIRE(r.has_value());
    }
    {
        UdpTracker udp2;
        AnnounceRequest req{};
        req.infoHash = make_infohash(0x45);
        req.peerId   = make_peerid(0x02);
        req.port     = 51413;
        req.left     = 2;
        req.event    = AnnounceEvent::started;
        req.numwant  = 1;
        req.key      = 2;
        auto r = udp2.announce(req, url);
        REQUIRE(r.has_value());
    }

    CHECK(server.connectCount() >= 2); // one per instance

    server.stop();
}
