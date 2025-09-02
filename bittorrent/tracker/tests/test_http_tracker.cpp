#include <catch2/catch_all.hpp>

#include <string>
#include <vector>
#include <array>
#include <optional>

#include "../include/http_tracker.hpp"
#include "../include/http_client.hpp"
#include "../../bencode/bencode.hpp"

using namespace bittorrent::tracker;

// ---------- Fake HTTP client -------------------------------------------------

struct CapturingHttp : IHttpClient {
    std::string last_url;
    int status{200};
    std::string body;

    Expected<HttpResponse> get(const std::string& url, int, int, bool) override {
        last_url = url;
        if (status >= 400) {
            return Expected<HttpResponse>::failure("HTTP status " + std::to_string(status));
        }
        return Expected<HttpResponse>::success({status, body});
    }
};

static std::string ben(const bencode::BencodeValue& v) {
    return bencode::BencodeParser::encode(v);
}

// make binary 20-byte array with 0..19
static std::array<std::uint8_t,20> seq20() {
    std::array<std::uint8_t,20> a{};
    for (size_t i=0;i<20;++i) a[i]=static_cast<std::uint8_t>(i);
    return a;
}

// ---------- URL building via captured URL -----------------------------------

TEST_CASE("announce() builds URL with percent-encoded info_hash and peer_id, and all params") {
    auto http = std::make_shared<CapturingHttp>();
    HttpTracker tracker(http);

    AnnounceRequest req{};
    req.infoHash.bytes = seq20();            // 00 01 02 ... 13
    req.peerId.bytes   = seq20();            // same sequence
    req.port = 51413;
    req.uploaded = 123;
    req.downloaded = 456;
    req.left = 789;
    req.event = AnnounceEvent::started;
    req.numwant = 33;
    req.key = 0xA1B2C3D4;
    req.compact = true;
    req.no_peer_id = true;
    req.ipv6 = std::string("fe80::1");
    req.trackerId = std::string("trkid-123");

    // Minimal valid announce body so parsing succeeds
    bencode::BencodeValue root(std::map<std::string,bencode::BencodeValue>{
        {"interval", (int64_t)60},
        {"peers",    std::string{}}});
    http->body = ben(root);

    auto r = tracker.announce(req, "http://example/announce");
    REQUIRE(r.has_value()); // not testing parse here, just URL capture

    // Check URL base and required params
    REQUIRE(http->last_url.find("http://example/announce?") == 0);

    // info_hash and peer_id should be percent-encoded (uppercase hex)
    std::string expected20;
    expected20.reserve(3*20);
    for (int i=0;i<20;++i) {
        char buf[4]; std::snprintf(buf, sizeof(buf), "%%%02X", i & 0xFF);
        expected20 += buf;
    }
    REQUIRE(http->last_url.find("info_hash=" + expected20) != std::string::npos);
    REQUIRE(http->last_url.find("peer_id=" + expected20)   != std::string::npos);

    // Basic numeric params
    REQUIRE(http->last_url.find("&port=51413")      != std::string::npos);
    REQUIRE(http->last_url.find("&uploaded=123")    != std::string::npos);
    REQUIRE(http->last_url.find("&downloaded=456")  != std::string::npos);
    REQUIRE(http->last_url.find("&left=789")        != std::string::npos);
    REQUIRE(http->last_url.find("&event=started")   != std::string::npos);
    REQUIRE(http->last_url.find("&compact=1")       != std::string::npos);
    REQUIRE(http->last_url.find("&numwant=33")      != std::string::npos);
    REQUIRE(http->last_url.find("&key=2712847316")  != std::string::npos); // 0xA1B2C3D4 in decimal

    REQUIRE(http->last_url.find("&no_peer_id=1")    != std::string::npos);
    // ipv6 must be percent-encoded: 'fe80::1' -> 'fe80%3A%3A1'
    REQUIRE(http->last_url.find("&ipv6=fe80%3A%3A1")!= std::string::npos);
    REQUIRE(http->last_url.find("&trackerid=trkid-123") != std::string::npos);
}

TEST_CASE("announce() appends with & when base URL already has ?") {
    auto http = std::make_shared<CapturingHttp>();
    HttpTracker tracker(http);

    AnnounceRequest req{};
    req.infoHash.bytes = seq20();
    req.peerId.bytes   = seq20();

    bencode::BencodeValue root(std::map<std::string,bencode::BencodeValue>{
        {"interval", (int64_t)60},
        {"peers",    std::string{}}});
    http->body = ben(root);

    auto r = tracker.announce(req, "http://host/path?x=1");
    REQUIRE(r.has_value());

    // Should have kept existing query and used '&' for new params
    REQUIRE(http->last_url.find("http://host/path?x=1&info_hash=") == 0);
}

// ---------- Announce response parsing --------------------------------------

TEST_CASE("announce parse: failure reason returns error") {
    auto http = std::make_shared<CapturingHttp>();
    HttpTracker tracker(http);

    bencode::BencodeValue root(std::map<std::string,bencode::BencodeValue>{
        {"failure reason", std::string("nope") }
    });
    http->body = ben(root);

    AnnounceRequest req{}; req.infoHash.bytes = seq20(); req.peerId.bytes = seq20();
    auto r = tracker.announce(req, "http://x/announce");
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error->message.find("nope") != std::string::npos);
}

TEST_CASE("announce parse: compact IPv4 peers + stats + tracker id + warning") {
    auto http = std::make_shared<CapturingHttp>();
    HttpTracker tracker(http);

    // peers: 1.2.3.4:6881
    std::string peers;
    peers.push_back(1); peers.push_back(2); peers.push_back(3); peers.push_back(4);
    peers.push_back((char)0x1A); peers.push_back((char)0xE1); // 6881

    bencode::BencodeValue root(std::map<std::string,bencode::BencodeValue>{
        {"interval",        (int64_t)1800},
        {"min interval",    (int64_t)900},
        {"complete",        (int64_t)10},
        {"incomplete",      (int64_t)5},
        {"warning message", std::string("be polite")},
        {"tracker id",      std::string("trk-42")},
        {"peers",           bencode::BencodeValue(peers)}
    });
    http->body = ben(root);

    AnnounceRequest req{}; req.infoHash.bytes = seq20(); req.peerId.bytes = seq20();
    auto r = tracker.announce(req, "http://t/announce");
    REQUIRE(r.has_value());

    const auto& a = r.get();
    REQUIRE(a.interval == 1800);
    REQUIRE(a.minInterval.has_value());
    REQUIRE(a.minInterval.value() == 900);
    REQUIRE(a.complete == 10);
    REQUIRE(a.incomplete == 5);
    REQUIRE(a.warning.has_value());
    REQUIRE(a.trackerId.has_value());
    REQUIRE(a.trackerId.value() == "trk-42");

    REQUIRE(a.peers.size() == 1);
    CHECK(a.peers[0].ip == "1.2.3.4");
    CHECK(a.peers[0].port == 6881);
}

TEST_CASE("announce parse: peers6 (compact IPv6)") {
    auto http = std::make_shared<CapturingHttp>();
    HttpTracker tracker(http);

    std::string peers6(18, '\0');
    peers6[15] = 1;
    peers6[16] = (char)(51413 >> 8);
    peers6[17] = (char)(51413 & 0xFF);

    bencode::BencodeValue root(std::map<std::string,bencode::BencodeValue>{
        {"interval", (int64_t)60},
        {"peers6",   bencode::BencodeValue(peers6)}
    });
    http->body = ben(root);

    AnnounceRequest req{}; req.infoHash.bytes = seq20(); req.peerId.bytes = seq20();
    auto r = tracker.announce(req, "http://t/announce");
    REQUIRE(r.has_value());
    REQUIRE(r.get().peers.size() == 1);
    CHECK(r.get().peers[0].ip == "::1");
    CHECK(r.get().peers[0].port == 51413);
}

TEST_CASE("announce parse: non-compact peer list of dicts") {
    auto http = std::make_shared<CapturingHttp>();
    HttpTracker tracker(http);

    using namespace bencode;
    BencodeValue peer1(std::map<std::string,BencodeValue>{
        {"ip",   BencodeValue(std::string("9.8.7.6"))},
        {"port", BencodeValue((int64_t)1234)}
    });
    BencodeValue peer2(std::map<std::string,BencodeValue>{
        {"ip",   BencodeValue(std::string("127.0.0.1"))},
        {"port", BencodeValue((int64_t)80)}
    });

    BencodeValue root(std::map<std::string,BencodeValue>{
        {"interval", BencodeValue((int64_t)60)},
        {"peers",    BencodeValue(std::vector<BencodeValue>{peer1, peer2})}
    });

    http->body = ben(root);

    AnnounceRequest req{}; req.infoHash.bytes = seq20(); req.peerId.bytes = seq20();
    auto r = tracker.announce(req, "http://t/announce");
    REQUIRE(r.has_value());
    const auto& peers = r.get().peers;
    REQUIRE(peers.size() == 2);
    CHECK(peers[0].ip == "9.8.7.6");
    CHECK(peers[0].port == 1234);
    CHECK(peers[1].ip == "127.0.0.1");
    CHECK(peers[1].port == 80);
}

TEST_CASE("announce parse: not a dict -> error") {
    auto http = std::make_shared<CapturingHttp>();
    HttpTracker tracker(http);

    // body is just an int: i123e
    http->body = ben(bencode::BencodeValue((int64_t)123));

    AnnounceRequest req{}; req.infoHash.bytes = seq20(); req.peerId.bytes = seq20();
    auto r = tracker.announce(req, "http://t/announce");
    REQUIRE_FALSE(r.has_value());
}

// ---------- Scrape parsing --------------------------------------------------

TEST_CASE("scrape parse: files dict with one torrent entry") {
    auto http = std::make_shared<CapturingHttp>();
    HttpTracker tracker(http);

    // Build files dict: key is 20-byte binary infohash; value is dict of stats
    std::array<std::uint8_t,20> key = seq20();
    std::string keyStr(reinterpret_cast<const char*>(key.data()), key.size());

    using namespace bencode;
    BencodeValue stats(std::map<std::string,BencodeValue>{
        {"complete",   BencodeValue((int64_t)7)},
        {"downloaded", BencodeValue((int64_t)42)},
        {"incomplete", BencodeValue((int64_t)3)},
        {"name",       BencodeValue(std::string("Ubuntu ISO"))}
    });

    BencodeValue files(std::map<std::string,BencodeValue>{
        { keyStr, stats }
    });

    BencodeValue root(std::map<std::string,BencodeValue>{
        {"files", files}
    });

    http->body = ben(root);

    auto r = tracker.scrape({InfoHash{key}}, "http://t/scrape");
    REQUIRE(r.has_value());

    auto& mp = r.get();
    REQUIRE(mp.size() == 1);
    auto it = mp.find(InfoHash{key});
    REQUIRE(it != mp.end());
    CHECK(it->second.complete == 7);
    CHECK(it->second.downloaded == 42);
    CHECK(it->second.incomplete == 3);
    REQUIRE(it->second.name.has_value());
    CHECK(it->second.name.value() == "Ubuntu ISO");
}

TEST_CASE("scrape parse: missing files dict -> error") {
    auto http = std::make_shared<CapturingHttp>();
    HttpTracker tracker(http);

    // root is an empty dict: d e
    http->body = ben(bencode::BencodeValue(std::map<std::string,bencode::BencodeValue>{}));

    auto r = tracker.scrape({}, "http://t/scrape");
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("scrape parse: non-dict root -> error") {
    auto http = std::make_shared<CapturingHttp>();
    HttpTracker tracker(http);

    // an integer instead of dict
    http->body = ben(bencode::BencodeValue((int64_t)1));

    auto r = tracker.scrape({}, "http://t/scrape");
    REQUIRE_FALSE(r.has_value());
}
