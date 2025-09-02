// tests/test_types.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

// Adjust include path to where your header lives:
// If your tree is include/bittorrent/tracker/include/types.hpp, you might need:
//   #include "bittorrent/tracker/include/types.hpp"
// or relative to this file:
//   #include "../include/types.hpp"
#include "../include/types.hpp"

using namespace bittorrent::tracker;

// Helper: format a byte to 2-char lowercase hex without iostreams
static std::string byte_to_hex(uint8_t b) {
    static const char* digits = "0123456789abcdef";
    std::string s;
    s.push_back(digits[(b >> 4) & 0xF]);
    s.push_back(digits[b & 0xF]);
    return s;
}

// Helper: reference formatter independent of InfoHash::toHex()
static std::string oracle_hex(const std::array<std::uint8_t,20>& bytes) {
    std::string out;
    out.reserve(40);
    for (auto b : bytes) out += byte_to_hex(b);
    return out;
}

TEST_CASE("InfoHash::toHex produces 40 lowercase hex characters") {
    InfoHash ih{};
    // Fill with a simple pattern
    for (size_t i = 0; i < ih.bytes.size(); ++i) ih.bytes[i] = static_cast<uint8_t>(i);

    const std::string hex = ih.toHex();

    REQUIRE(hex.size() == 40);
    auto is_hex_lower = [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    };
    for (char c : hex) {
        REQUIRE( is_hex_lower(c) );
    }

}

TEST_CASE("InfoHash::toHex preserves leading zeros") {
    InfoHash ih{};
    ih.bytes.fill(0);
    // set a few with leading-zero nibbles
    ih.bytes[0] = 0x00;
    ih.bytes[1] = 0x01;
    ih.bytes[2] = 0x0a; // -> "0a"
    ih.bytes[3] = 0x10; // -> "10"

    const std::string hex = ih.toHex();

    REQUIRE(hex.rfind("00010a10", 0) == 0); // starts with exact sequence
    REQUIRE(hex.size() == 40);
}

TEST_CASE("InfoHash::toHex exact mapping for boundary bytes") {
    InfoHash ih{};
    // Construct a repeating pattern: 00, 7f, 80, ff, then zeros
    ih.bytes[0] = 0x00;
    ih.bytes[1] = 0x7f;
    ih.bytes[2] = 0x80;
    ih.bytes[3] = 0xff;

    const std::string hex = ih.toHex();

    REQUIRE(hex.substr(0, 8) == std::string("007f80ff"));
}

TEST_CASE("InfoHash::toHex matches independent oracle") {
    InfoHash ih{};
    for (size_t i = 0; i < ih.bytes.size(); ++i)
        ih.bytes[i] = static_cast<uint8_t>(i * 7 + 3); // arbitrary pattern

    const std::string hex = ih.toHex();
    const std::string ref = oracle_hex(ih.bytes);

    REQUIRE(hex == ref);
}

TEST_CASE("InfoHash equality and ordering (operator<=> default)") {
    InfoHash a{}, b{};

    // same content => equal
    for (size_t i = 0; i < a.bytes.size(); ++i) {
        a.bytes[i] = static_cast<uint8_t>(i);
        b.bytes[i] = static_cast<uint8_t>(i);
    }
    REQUIRE((a <=> b) == std::strong_ordering::equal);

    // make b slightly larger at the last byte
    b.bytes.back() = static_cast<uint8_t>(a.bytes.back() + 1);

    REQUIRE((a <=> b) == std::strong_ordering::less);
    REQUIRE((b <=> a) == std::strong_ordering::greater);
}
