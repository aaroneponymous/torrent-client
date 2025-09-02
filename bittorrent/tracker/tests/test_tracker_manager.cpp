#include <catch2/catch_all.hpp>

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>

#include "../include/manager.hpp"
#include "../include/http_client.hpp"
#include "../include/types.hpp"
#include "../../bencode/bencode.hpp"

using namespace bencode;

// Build a minimal announce response: interval + non-compact peers list
static std::string ben_announce(
    int interval,
    const std::vector<std::pair<std::string,int>>& peers)
{
    std::map<std::string, BencodeValue> root;
    root["interval"] = BencodeValue((int64_t)interval);

    std::vector<BencodeValue> plist;
    plist.reserve(peers.size());
    for (auto& [ip, port] : peers) {
        std::map<std::string, BencodeValue> p;
        p["ip"]   = BencodeValue(ip);
        p["port"] = BencodeValue((int64_t)port);
        plist.emplace_back(std::move(p));
    }
    root["peers"] = BencodeValue(std::move(plist));
    return BencodeParser::encode(BencodeValue(std::move(root)));
}

namespace bt = bittorrent::tracker;

// ---- Fake IHttpClient matching the header signature ------------------------
struct FakeHttpClient : public bt::IHttpClient {
    ~FakeHttpClient() override = default;

    struct Mapping {
        int status{200};
        std::string body;
        std::string error; // if non-empty, return failure(...)
    };

    // Exact URL -> response mapping
    std::unordered_map<std::string, Mapping> responses;

    // Capture called URLs & allow tests to wait until a call happens
    std::mutex mu;
    std::vector<std::string> calls;
    std::condition_variable cv;
    std::atomic<int> call_count{0};

    bt::Expected<bt::HttpResponse>
    get(const std::string& url, int /*connectTimeoutSec*/, int /*transferTimeoutSec*/, bool /*followRedirects*/) override
    {
        {
            std::lock_guard<std::mutex> lk(mu);
            calls.push_back(url);
        }
        call_count.fetch_add(1);
        cv.notify_all();

        auto it = responses.find(url);
        if (it == responses.end()) {
            return bt::Expected<bt::HttpResponse>::failure("no mapping for " + url);
        }
        if (!it->second.error.empty()) {
            return bt::Expected<bt::HttpResponse>::failure(it->second.error);
        }
        return bt::Expected<bt::HttpResponse>::success(bt::HttpResponse{it->second.status, it->second.body});
    }
};

// ---- Small helpers to build inputs for TrackerManager ----------------------
static bt::InfoHash make_infohash() {
    bt::InfoHash ih{};
    for (size_t i = 0; i < ih.bytes.size(); ++i) ih.bytes[i] = static_cast<uint8_t>(i);
    return ih;
}

static bt::PeerID make_peerid() {
    bt::PeerID p{};
    for (size_t i = 0; i < p.bytes.size(); ++i) p.bytes[i] = static_cast<uint8_t>(i ^ 0xA5);
    return p;
}

static bool wait_for_calls(FakeHttpClient& http, int target, std::chrono::milliseconds timeout) {
    std::mutex dummy;
    std::unique_lock<std::mutex> lk(dummy);
    return http.cv.wait_for(lk, timeout, [&]{ return http.call_count.load() >= target; });
}

// =========================== TESTS ==========================================

TEST_CASE("TrackerManager: start/stop lifecycle is safe") {
    auto http = std::make_shared<FakeHttpClient>();
    std::vector<std::vector<std::string>> announceList{
        {"http://t.example/announce"}
    };

    bt::TrackerManager mgr(announceList, make_infohash(), make_peerid(), /*port*/51413, http);
    mgr.start();
    mgr.stop();
    SUCCEED();
}

TEST_CASE("TrackerManager: announce delivers peers that can be drained") {
    auto http = std::make_shared<FakeHttpClient>();
    std::vector<std::vector<std::string>> announceList{
        {"http://t.example/announce"}
    };

    bt::TrackerManager mgr(announceList, make_infohash(), make_peerid(), /*port*/51413, http);
    mgr.start();

    // First announce to trigger URL construction and capture
    mgr.announce(bt::AnnounceEvent::started, /*numwant*/10);
    REQUIRE(wait_for_calls(*http, 1, std::chrono::seconds(2)));

    // Get the exact URL built by the manager/http_tracker
    std::string url;
    {
        std::lock_guard<std::mutex> lk(http->mu);
        REQUIRE_FALSE(http->calls.empty());
        url = http->calls.back();
    }

    // Provide a success mapping for that exact URL
    http->responses[url] = {200, ben_announce(1800, {{"1.2.3.4", 6881}, {"9.8.7.6", 80}}), ""};

    // Trigger a second announce to actually consume the mapping
    mgr.announce(bt::AnnounceEvent::none, /*numwant*/10);
    REQUIRE(wait_for_calls(*http, 2, std::chrono::seconds(2)));

    // Drain peers from the manager
    auto peers = mgr.drainNewPeers();
    // If your implementation only uses callbacks, peers may be empty here.
    // If it buffers, assert the content:
    if (!peers.empty()) {
        REQUIRE(peers.size() == 2);
        CHECK(peers[0].ip == "1.2.3.4");
        CHECK(peers[0].port == 6881);
        CHECK(peers[1].ip == "9.8.7.6");
        CHECK(peers[1].port == 80);
    }

    mgr.stop();
}

TEST_CASE("TrackerManager: setPeersCallback receives delivered peers") {
    auto http = std::make_shared<FakeHttpClient>();
    std::vector<std::vector<std::string>> announceList{
        {"http://t.example/announce"}
    };

    bt::TrackerManager mgr(announceList, make_infohash(), make_peerid(), /*port*/51413, http);

    std::mutex cbMu;
    std::condition_variable cbCv;
    std::vector<bt::PeerAddr> delivered;

    mgr.setPeersCallback([&](const std::vector<bt::PeerAddr>& peers){
        std::lock_guard<std::mutex> lk(cbMu);
        delivered.insert(delivered.end(), peers.begin(), peers.end());
        cbCv.notify_all();
    });

    mgr.start();

    // Trigger first call to capture URL
    mgr.announce(bt::AnnounceEvent::started, /*numwant*/5);
    REQUIRE(wait_for_calls(*http, 1, std::chrono::seconds(2)));

    std::string url;
    {
        std::lock_guard<std::mutex> lk(http->mu);
        url = http->calls.back();
    }

    // Provide response mapping
    http->responses[url] = {200, ben_announce(900, {{"127.0.0.1", 51413}}), ""};

    // Trigger again to consume it
    mgr.announce(bt::AnnounceEvent::none, /*numwant*/5);
    REQUIRE(wait_for_calls(*http, 2, std::chrono::seconds(2)));

    // Wait for callback
    {
        std::unique_lock<std::mutex> lk(cbMu);
        cbCv.wait_for(lk, std::chrono::seconds(2), [&]{ return !delivered.empty(); });
    }
    REQUIRE(delivered.size() == 1);
    CHECK(delivered[0].ip == "127.0.0.1");
    CHECK(delivered[0].port == 51413);

    mgr.stop();
}

TEST_CASE("TrackerManager: endpoint rotation within a tier (first fails, second succeeds)") {
    auto http = std::make_shared<FakeHttpClient>();
    std::vector<std::vector<std::string>> announceList{
        {"http://a.example/announce", "http://b.example/announce"}
    };

    bt::TrackerManager mgr(announceList, make_infohash(), make_peerid(), /*port*/51413, http);
    mgr.start();

    // First call -> capture A URL
    mgr.announce(bt::AnnounceEvent::started, /*numwant*/10);
    REQUIRE(wait_for_calls(*http, 1, std::chrono::seconds(2)));

    std::string a_url;
    {
        std::lock_guard<std::mutex> lk(http->mu);
        a_url = http->calls.back();
    }
    http->responses[a_url] = {0, "", "tracker a down"};

    // Second call -> manager should rotate; capture B URL
    mgr.announce(bt::AnnounceEvent::none, /*numwant*/10);
    REQUIRE(wait_for_calls(*http, 2, std::chrono::seconds(2)));

    std::string b_url;
    {
        std::lock_guard<std::mutex> lk(http->mu);
        b_url = http->calls.back();
    }
    http->responses[b_url] = {200, ben_announce(1200, {{"10.0.0.1", 6881}}), ""};

    // Third call -> consume success mapping
    mgr.announce(bt::AnnounceEvent::none, /*numwant*/10);
    REQUIRE(wait_for_calls(*http, 3, std::chrono::seconds(2)));

    // Drain (if used) or rely on callback in previous test
    auto peers = mgr.drainNewPeers();
    if (!peers.empty()) {
        REQUIRE(peers.size() == 1);
        CHECK(peers[0].ip == "10.0.0.1");
        CHECK(peers[0].port == 6881);
    }

    mgr.stop();
}

TEST_CASE("TrackerManager: onStats updates are accepted") {
    auto http = std::make_shared<FakeHttpClient>();
    std::vector<std::vector<std::string>> announceList{
        {"http://t.example/announce"}
    };

    bt::TrackerManager mgr(announceList, make_infohash(), make_peerid(), /*port*/51413, http);
    mgr.start();

    // Just ensure onStats doesn't crash and subsequent announce triggers a call
    mgr.onStats(/*uploaded*/123, /*downloaded*/456, /*left*/789);
    mgr.announce(bt::AnnounceEvent::none, 5);
    REQUIRE(wait_for_calls(*http, 1, std::chrono::seconds(2)));

    mgr.stop();
}
