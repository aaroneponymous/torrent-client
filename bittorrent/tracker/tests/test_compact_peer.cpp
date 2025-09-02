#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

#include <arpa/inet.h>   // inet_pton, htons
#include <string>
#include <vector>
#include <cstring>

#include "../include/compact_peer_codec.hpp"

using namespace bittorrent::tracker;

// --- helpers --------------------------------------------------------------

static std::string build_ipv4_blob(
    const std::vector<std::pair<std::array<uint8_t,4>, uint16_t>>& peers) {
    std::string raw;
    raw.reserve(peers.size() * 6);
    for (auto const& [ip, port] : peers) {
        raw.push_back(static_cast<char>(ip[0]));
        raw.push_back(static_cast<char>(ip[1]));
        raw.push_back(static_cast<char>(ip[2]));
        raw.push_back(static_cast<char>(ip[3]));
        // Write network order directly: high byte then low byte
        raw.push_back(static_cast<char>((port >> 8) & 0xFF));
        raw.push_back(static_cast<char>(port & 0xFF));
    }
    return raw;
}

static std::string build_ipv6_blob(
    const std::vector<std::pair<std::array<uint8_t,16>, uint16_t>>& peers) {
    std::string raw;
    raw.reserve(peers.size() * 18);
    for (auto const& [ip, port] : peers) {
        for (uint8_t b : ip) raw.push_back(static_cast<char>(b));
        // Network order: high byte then low byte
        raw.push_back(static_cast<char>((port >> 8) & 0xFF));
        raw.push_back(static_cast<char>(port & 0xFF));
    }
    return raw;
}


static std::array<uint8_t,4> v4(const char* dotted) {
    std::array<uint8_t,4> out{};
    REQUIRE(inet_pton(AF_INET, dotted, out.data()) == 1);
    return out;
}

static std::array<uint8_t,16> v6(const char* text) {
    std::array<uint8_t,16> out{};
    REQUIRE(inet_pton(AF_INET6, text, out.data()) == 1);
    return out;
}

// --- tests: IPv4 ----------------------------------------------------------

TEST_CASE("IPv4: empty input yields empty vector") {
    auto peers = CompactPeerCodec::parseIPv4(std::string_view{});
    REQUIRE(peers.empty());
}

TEST_CASE("IPv4: length not divisible by 6 returns empty") {
    std::string garbage = "abcdef"; // 6 bytes
    garbage.push_back('X');         // 7th byte breaks divisibility
    auto peers = CompactPeerCodec::parseIPv4(garbage);
    REQUIRE(peers.empty());
}

TEST_CASE("IPv4: single peer parses correctly") {
    auto raw = build_ipv4_blob({{ v4("1.2.3.4"), 6881 }});
    auto peers = CompactPeerCodec::parseIPv4(raw);
    REQUIRE(peers.size() == 1);
    REQUIRE(peers[0].ip == "1.2.3.4");
    REQUIRE(peers[0].port == 6881);
}

TEST_CASE("IPv4: multiple peers preserve order") {
    auto raw = build_ipv4_blob({
        { v4("1.1.1.1"), 6881 },
        { v4("8.8.8.8"), 53   },
        { v4("127.0.0.1"), 80 }
    });
    auto peers = CompactPeerCodec::parseIPv4(raw);
    REQUIRE(peers.size() == 3);
    CHECK(peers[0].ip == "1.1.1.1");
    CHECK(peers[0].port == 6881);
    CHECK(peers[1].ip == "8.8.8.8");
    CHECK(peers[1].port == 53);
    CHECK(peers[2].ip == "127.0.0.1");
    CHECK(peers[2].port == 80);
}

TEST_CASE("IPv4: boundary ports and addresses") {
    auto raw = build_ipv4_blob({
        { v4("0.0.0.0"),     0     },
        { v4("255.255.255.255"), 65535 },
        { v4("192.168.0.1"),  1     },
        { v4("10.0.0.1"),     51413 }
    });
    auto peers = CompactPeerCodec::parseIPv4(raw);
    REQUIRE(peers.size() == 4);
    CHECK(peers[0].ip == "0.0.0.0");
    CHECK(peers[0].port == 0);
    CHECK(peers[1].ip == "255.255.255.255");
    CHECK(peers[1].port == 65535);
    CHECK(peers[2].ip == "192.168.0.1");
    CHECK(peers[2].port == 1);
    CHECK(peers[3].ip == "10.0.0.1");
    CHECK(peers[3].port == 51413);
}

// --- tests: IPv6 ----------------------------------------------------------

TEST_CASE("IPv6: empty input yields empty vector") {
    auto peers = CompactPeerCodec::parseIPv6(std::string_view{});
    REQUIRE(peers.empty());
}

TEST_CASE("IPv6: length not divisible by 18 returns empty") {
    std::string raw(18, '\0'); // 18 bytes ok
    raw.push_back('x');        // 19 breaks it
    auto peers = CompactPeerCodec::parseIPv6(raw);
    REQUIRE(peers.empty());
}

TEST_CASE("IPv6: single peer ::1 parsing and port") {
    std::array<uint8_t,16> addr{};
    addr[15] = 1; // ::1
    auto raw = build_ipv6_blob({ { addr, 51413 } });
    auto peers = CompactPeerCodec::parseIPv6(raw);
    REQUIRE(peers.size() == 1);
    // inet_ntop should compress ::1
    CHECK(peers[0].ip == "::1");
    CHECK(peers[0].port == 51413);
}

TEST_CASE("IPv6: multiple peers preserve order with typical addresses") {
    auto raw = build_ipv6_blob({
        { v6("2001:db8::1"),  443   },
        { v6("fe80::1"),      80    },
        { v6("::ffff:192.0.2.128"), 6881 } // v4-mapped
    });
    auto peers = CompactPeerCodec::parseIPv6(raw);
    REQUIRE(peers.size() == 3);

    // Formatting from inet_ntop is canonical compressed; match known forms:
    CHECK(peers[0].ip == "2001:db8::1");
    CHECK(peers[0].port == 443);

    CHECK(peers[1].ip == "fe80::1");
    CHECK(peers[1].port == 80);

    // inet_ntop for v4-mapped should render "::ffff:192.0.2.128"
    CHECK(peers[2].ip == "::ffff:192.0.2.128");
    CHECK(peers[2].port == 6881);
}

TEST_CASE("IPv6: boundary ports 0 and 65535") {
    auto raw = build_ipv6_blob({
        { v6("2001:db8::dead:beef"), 0 },
        { v6("2001:db8::dead:beef"), 65535 }
    });
    auto peers = CompactPeerCodec::parseIPv6(raw);
    REQUIRE(peers.size() == 2);
    CHECK(peers[0].port == 0);
    CHECK(peers[1].port == 65535);
}

// --- tests: robustness ----------------------------------------------------

TEST_CASE("Robustness: random non-printable bytes do not crash and parse correctly by length") {
    // Length multiple of 6 → parsed as IPv4 items, regardless of content.
    std::string raw_v4(12, '\xFF');
    auto v4peers = CompactPeerCodec::parseIPv4(raw_v4);
    REQUIRE(v4peers.size() == 2);

    // Length multiple of 18 → parsed as IPv6 items.
    std::string raw_v6(36, '\xAB');
    auto v6peers = CompactPeerCodec::parseIPv6(raw_v6);
    REQUIRE(v6peers.size() == 2);
}

TEST_CASE("Determinism: same input yields identical output") {
    auto raw = build_ipv4_blob({
        { v4("11.22.33.44"), 1234 },
        { v4("55.66.77.88"), 65535 }
    });
    auto a = CompactPeerCodec::parseIPv4(raw);
    auto b = CompactPeerCodec::parseIPv4(raw);
    REQUIRE(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].ip == b[i].ip);
        CHECK(a[i].port == b[i].port);
    }
}
