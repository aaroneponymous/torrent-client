#include <thread>
#include "udp_tracker.hpp"
#include "udp_url.hpp"



namespace bittorrent::tracker {


    /**
     * @todo: Future Additon
     * - Cache connection per (host,port) across calls; refresh on expiry.
     * - Non-blocking socket + poll for tighter control.
     * - IPv6 peers + v2 protocol handling if you encounter trackers that support it.
     * - EDNS/DoH name resolution (if you want to avoid blocking getaddrinfo on UI threads).
     * - Stats: emit per-attempt latency to drive smarter backoff across tiers.
    */

    using namespace std::chrono;

    // ---------- Binary helpers ----------
    inline void UdpTracker::put_u16(std::vector<uint8_t>& b, uint16_t v) {
        b.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        b.push_back(static_cast<uint8_t>(v & 0xFF));
    }
    inline void UdpTracker::put_u32(std::vector<uint8_t>& b, uint32_t v) {
        b.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
        b.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        b.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        b.push_back(static_cast<uint8_t>(v & 0xFF));
    }
    inline void UdpTracker::put_u64(std::vector<uint8_t>& b, uint64_t v) {
        put_u32(b, static_cast<uint32_t>(v >> 32));
        put_u32(b, static_cast<uint32_t>(v & 0xFFFFFFFFu));
    }
    inline uint16_t UdpTracker::get_u16(const uint8_t* p) {
        return (uint16_t(p[0]) << 8) | uint16_t(p[1]);
    }
    inline uint32_t UdpTracker::get_u32(const uint8_t* p) {
        return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
    }
    inline uint64_t UdpTracker::get_u64(const uint8_t* p) {
        return (uint64_t(get_u32(p)) << 32) | get_u32(p + 4);
    }

    uint32_t UdpTracker::rand_u32() {
        static thread_local std::mt19937 rng([]{
            std::random_device rd;
            std::seed_seq ss{rd(), rd(), rd(), rd(), rd(), rd()};
            return std::mt19937{ss};
        }());
        return std::uniform_int_distribution<uint32_t>{}(rng);
    }

    // ---------- URL parsing and DNS ----------
    Expected<std::pair<sockaddr_storage, socklen_t>>
    UdpTracker::parseUdpUrl(const std::string& url)
    {
        using detail::parse_udp_url_minimal;
        auto partsOpt = parse_udp_url_minimal(url);
        if (!partsOpt.has_value()) {
            return Expected<std::pair<sockaddr_storage, socklen_t>>::failure(
                "udp: invalid URL (expect udp://host[:port]/...)");
        }
        const auto& parts = *partsOpt;

        struct addrinfo hints{};
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;
        // Prefer IPv4 first for compact peers (Phase 1). If that fails, allow any:
        hints.ai_family = AF_INET;

        struct addrinfo* res = nullptr;
        int rc = ::getaddrinfo(parts.host.c_str(), std::to_string(parts.port).c_str(), &hints, &res);
        if (rc != 0) {
            hints.ai_family = AF_UNSPEC;
            rc = ::getaddrinfo(parts.host.c_str(), std::to_string(parts.port).c_str(), &hints, &res);
            if (rc != 0) {
                return Expected<std::pair<sockaddr_storage, socklen_t>>::failure(
                    std::string("udp: getaddrinfo failed: ") + gai_strerror(rc));
            }
        }

        sockaddr_storage ss{};
        socklen_t slen = 0;

        if (res->ai_addrlen > sizeof(ss)) {
            ::freeaddrinfo(res);
            return Expected<std::pair<sockaddr_storage, socklen_t>>::failure("udp: resolved addr too large");
        }
        std::memcpy(&ss, res->ai_addr, res->ai_addrlen);
        slen = static_cast<socklen_t>(res->ai_addrlen);
        ::freeaddrinfo(res);

        return Expected<std::pair<sockaddr_storage, socklen_t>>::success(std::make_pair(ss, slen));
    }

    Expected<int> UdpTracker::makeUdpSocket(int family, std::chrono::milliseconds recvTimeout)
    {
        int fd = ::socket(family, SOCK_DGRAM, 0);
        if (fd < 0) {
            return Expected<int>::failure("udp: socket() failed");
        }

        // Set SO_RCVTIMEO
        timeval tv{};
        tv.tv_sec  = static_cast<time_t>(recvTimeout.count() / 1000);
        tv.tv_usec = static_cast<suseconds_t>((recvTimeout.count() % 1000) * 1000);
        if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            ::close(fd);
            return Expected<int>::failure("udp: setsockopt(SO_RCVTIMEO) failed");
        }

        return Expected<int>::success(fd);
    }

    // ---------- UDP Connect (action=0) ----------
    Expected<std::pair<uint64_t, std::chrono::steady_clock::time_point>>
    UdpTracker::connectAndGetConnId(const sockaddr_storage& addr, socklen_t addrlen, int sock,
                                    std::chrono::milliseconds /*timeoutPerAttempt*/,
                                    int maxAttempts, std::chrono::milliseconds backoffStart)
    {
        auto backoff = backoffStart;

        for (int attempt = 0; attempt < maxAttempts; ++attempt) {
            std::vector<uint8_t> buf;
            constexpr uint64_t proto = 0x41727101980ULL; // magic protocol id
            const uint32_t tx = rand_u32();

            put_u64(buf, proto);
            put_u32(buf, 0u);  // action = connect
            put_u32(buf, tx);

            if (::sendto(sock, buf.data(), buf.size(), 0, reinterpret_cast<const sockaddr*>(&addr), addrlen) < 0) {
                // transient; retry
            } else {
                uint8_t rbuf[2048];
                const ssize_t n = ::recvfrom(sock, rbuf, sizeof(rbuf), 0, nullptr, nullptr);
                if (n >= 16) {
                    uint32_t action = get_u32(rbuf + 0);
                    uint32_t rtx    = get_u32(rbuf + 4);
                    if (action == 3u) {
                        std::string msg(reinterpret_cast<char*>(rbuf) + 8, static_cast<size_t>(n - 8));
                        return Expected<std::pair<uint64_t, std::chrono::steady_clock::time_point>>::failure("udp error: " + msg);
                    }
                    if (action == 0u && rtx == tx) {
                        uint64_t cid = get_u64(rbuf + 8);
                        auto exp = std::chrono::steady_clock::now() + kConnTtl;
                        return Expected<std::pair<uint64_t, std::chrono::steady_clock::time_point>>::success(std::make_pair(cid, exp));
                    }
                    // mismatch: keep trying this attempt window
                }
                // short or timeout -> retry
            }
            std::this_thread::sleep_for(backoff);
            backoff *= 2;
        }

        return Expected<std::pair<uint64_t, std::chrono::steady_clock::time_point>>::failure("udp: connect exhausted retries");
    }

    // ---------- Announce ----------
    Expected<AnnounceResponse>
    UdpTracker::doAnnounce(const sockaddr_storage& addr, socklen_t addrlen, int sock,
                        const AnnounceRequest& req,
                        std::chrono::milliseconds /*timeoutPerAttempt*/,
                        int maxAttempts, std::chrono::milliseconds backoffStart)
    {
        auto backoff = backoffStart;

        for (int attempt = 0; attempt < maxAttempts; ++attempt) {
            // Ensure we have a fresh connection
            if (connId_ == 0 || std::chrono::steady_clock::now() >= connExpiry_) {
                auto c = connectAndGetConnId(addr, addrlen, sock, kTimeout, kMaxAttempts, kBackoff);
                if (!c.has_value()) {
                    return Expected<AnnounceResponse>::failure(
                        c.error.has_value() ? c.error->message : "udp: connect failed");
                }
                connId_ = c.get().first;
                connExpiry_ = c.get().second;
            }

            std::vector<uint8_t> b;
            const uint32_t tx = rand_u32();

            // header
            put_u64(b, connId_);
            put_u32(b, 1u); // action=announce
            put_u32(b, tx);

            // body
            b.insert(b.end(), req.infoHash.bytes.begin(), req.infoHash.bytes.end());
            b.insert(b.end(), req.peerId.bytes.begin(),  req.peerId.bytes.end());

            put_u64(b, req.downloaded);
            put_u64(b, req.left);
            put_u64(b, req.uploaded);

            uint32_t ev = 0u;
            switch (req.event) {
                case AnnounceEvent::started:   ev = 1u; break;
                case AnnounceEvent::completed: ev = 2u; break;
                case AnnounceEvent::stopped:   ev = 3u; break;
                default:                       ev = 0u; break;
            }
            put_u32(b, ev);
            put_u32(b, 0u);            // ip=0 (tracker detects)
            put_u32(b, req.key);
            put_u32(b, (req.numwant == 0) ? 0xFFFFFFFFu : req.numwant);
            put_u16(b, req.port);

            if (::sendto(sock, b.data(), b.size(), 0, reinterpret_cast<const sockaddr*>(&addr), addrlen) < 0) {
                // transient; retry
            } else {
                std::vector<uint8_t> rbuf(65536);
                const ssize_t n = ::recvfrom(sock, rbuf.data(), static_cast<int>(rbuf.size()), 0, nullptr, nullptr);
                if (n >= 20) {
                    uint32_t action = get_u32(rbuf.data() + 0);
                    uint32_t rtx    = get_u32(rbuf.data() + 4);

                    if (action == 3u) {
                        std::string msg(reinterpret_cast<char*>(rbuf.data()) + 8, static_cast<size_t>(n - 8));
                        connId_ = 0;
                        return Expected<AnnounceResponse>::failure("udp error: " + msg);
                    }
                    if (action == 1u && rtx == tx) {
                        AnnounceResponse out{};
                        out.interval   = get_u32(rbuf.data() + 8);
                        out.incomplete = get_u32(rbuf.data() + 12);
                        out.complete   = get_u32(rbuf.data() + 16);

                        size_t off = 20;
                        while (off + 6 <= static_cast<size_t>(n)) {
                            uint32_t ipn  = get_u32(rbuf.data() + off + 0);
                            uint16_t port = get_u16(rbuf.data() + off + 4);

                            std::array<unsigned char,4> ipb {
                                static_cast<unsigned char>((ipn >> 24) & 0xFF),
                                static_cast<unsigned char>((ipn >> 16) & 0xFF),
                                static_cast<unsigned char>((ipn >> 8) & 0xFF),
                                static_cast<unsigned char>(ipn & 0xFF)
                            };
                            char ipstr[INET_ADDRSTRLEN];
                            std::string ip;
                            if (::inet_ntop(AF_INET, ipb.data(), ipstr, sizeof(ipstr))) {
                                ip = ipstr;
                            } else {
                                ip = std::to_string(ipb[0]) + "." + std::to_string(ipb[1]) + "." +
                                    std::to_string(ipb[2]) + "." + std::to_string(ipb[3]);
                            }

                            out.peers.push_back(PeerAddr{ip, port, std::nullopt});
                            off += 6;
                        }
                        return Expected<AnnounceResponse>::success(std::move(out));
                    }
                    // mismatch; retry
                } else if (n >= 8) {
                    uint32_t action = get_u32(rbuf.data() + 0);
                    if (action == 3u) {
                        std::string msg(reinterpret_cast<char*>(rbuf.data()) + 8, static_cast<size_t>(n - 8));
                        connId_ = 0;
                        return Expected<AnnounceResponse>::failure("udp error: " + msg);
                    }
                }
                // timeout/short; mark connection as stale to force reconnect next attempt
                connId_ = 0;
            }

            std::this_thread::sleep_for(backoff);
            backoff *= 2;
        }

        return Expected<AnnounceResponse>::failure("udp: announce exhausted retries");
    }

    // ---------- Scrape ----------
    Expected<std::map<InfoHash, ScrapeStats>>
    UdpTracker::doScrape(const sockaddr_storage& addr, socklen_t addrlen, int sock,
                        const std::vector<InfoHash>& hashes,
                        std::chrono::milliseconds /*timeoutPerAttempt*/,
                        int maxAttempts, std::chrono::milliseconds backoffStart)
    {
        if (hashes.empty()) {
            return Expected<std::map<InfoHash, ScrapeStats>>::success({});
        }

        auto backoff = backoffStart;

        for (int attempt = 0; attempt < maxAttempts; ++attempt) {
            if (connId_ == 0 || std::chrono::steady_clock::now() >= connExpiry_) {
                auto c = connectAndGetConnId(addr, addrlen, sock, kTimeout, kMaxAttempts, kBackoff);
                if (!c.has_value()) {
                    return Expected<std::map<InfoHash, ScrapeStats>>::failure(
                        c.error.has_value() ? c.error->message : "udp: connect failed");
                }
                connId_ = c.get().first;
                connExpiry_ = c.get().second;
            }

            std::vector<uint8_t> b;
            const uint32_t tx = rand_u32();

            put_u64(b, connId_);
            put_u32(b, 2u); // action=scrape
            put_u32(b, tx);

            for (const auto& h : hashes) {
                b.insert(b.end(), h.bytes.begin(), h.bytes.end());
            }

            if (::sendto(sock, b.data(), b.size(), 0, reinterpret_cast<const sockaddr*>(&addr), addrlen) < 0) {
                // transient; retry
            } else {
                std::vector<uint8_t> rbuf(8192);
                const ssize_t n = ::recvfrom(sock, rbuf.data(), static_cast<int>(rbuf.size()), 0, nullptr, nullptr);
                if (n >= 8) {
                    uint32_t action = get_u32(rbuf.data() + 0);
                    uint32_t rtx    = get_u32(rbuf.data() + 4);

                    if (action == 3u) {
                        std::string msg(reinterpret_cast<char*>(rbuf.data()) + 8, static_cast<size_t>(n - 8));
                        connId_ = 0;
                        return Expected<std::map<InfoHash, ScrapeStats>>::failure("udp error: " + msg);
                    }
                    if (action == 2u && rtx == tx) {
                        size_t need = 8 + 12 * hashes.size();
                        if (static_cast<size_t>(n) < need) {
                            connId_ = 0;
                            return Expected<std::map<InfoHash, ScrapeStats>>::failure("udp: short scrape response");
                        }
                        std::map<InfoHash, ScrapeStats> out;
                        size_t off = 8;
                        for (const auto& h : hashes) {
                            ScrapeStats s{};
                            s.complete   = get_u32(rbuf.data() + off + 0);
                            s.downloaded = get_u32(rbuf.data() + off + 4);
                            s.incomplete = get_u32(rbuf.data() + off + 8);
                            out.emplace(h, s);
                            off += 12;
                        }
                        return Expected<std::map<InfoHash, ScrapeStats>>::success(std::move(out));
                    }
                    // mismatch; retry
                }
                connId_ = 0;
            }

            std::this_thread::sleep_for(backoff);
            backoff *= 2;
        }

        return Expected<std::map<InfoHash, ScrapeStats>>::failure("udp: scrape exhausted retries");
    }

    // ---------- Public entry points ----------
    Expected<AnnounceResponse>
    UdpTracker::announce(const AnnounceRequest& req, const std::string& url)
    {
        auto addr = parseUdpUrl(url);
        if (!addr.has_value())
            return Expected<AnnounceResponse>::failure(addr.error.has_value() ? addr.error->message : "udp: bad URL");

        auto [ss, slen] = addr.get();
        auto sock = makeUdpSocket(ss.ss_family, kTimeout);
        if (!sock.has_value())
            return Expected<AnnounceResponse>::failure(sock.error.has_value() ? sock.error->message : "udp: socket failed");

        int fd = sock.get();
        auto res = doAnnounce(ss, slen, fd, req, kTimeout, kMaxAttempts, kBackoff);
        ::close(fd);
        return res;
    }

    Expected<std::map<InfoHash, ScrapeStats>>
    UdpTracker::scrape(const std::vector<InfoHash>& hashes, const std::string& url)
    {
        auto addr = parseUdpUrl(url);
        if (!addr.has_value())
            return Expected<std::map<InfoHash, ScrapeStats>>::failure(addr.error.has_value() ? addr.error->message : "udp: bad URL");

        auto [ss, slen] = addr.get();
        auto sock = makeUdpSocket(ss.ss_family, kTimeout);
        if (!sock.has_value())
            return Expected<std::map<InfoHash, ScrapeStats>>::failure(sock.error.has_value() ? sock.error->message : "udp: socket failed");

        int fd = sock.get();
        auto res = doScrape(ss, slen, fd, hashes, kTimeout, kMaxAttempts, kBackoff);
        ::close(fd);
        return res;
    }

} // namespace bittorrent::tracker
