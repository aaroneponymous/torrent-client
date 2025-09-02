#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <compare>


namespace bittorrent::tracker {


    enum class AnnounceEvent { none, started, completed, stopped };

    enum class Scheme { http, https, udp };

    struct InfoHash 
    {
        std::array<std::uint8_t,20> bytes{};
        std::string toHex() const;
        auto operator<=>(const InfoHash&) const = default;
    };


    struct PeerID {
    std::array<std::uint8_t,20> bytes{};
    };


    struct PeerAddr 
    {
        std::string ip; // dotted or RFC5952
        std::uint16_t port{0};
        std::optional<std::array<std::uint8_t,20>> peerId; // rarely present w/ non-compact
    };


    struct AnnounceRequest 
    {
        InfoHash infoHash;
        PeerID peerId;
        std::uint16_t port{6881};
        std::uint64_t uploaded{0};
        std::uint64_t downloaded{0};
        std::uint64_t left{0};
        AnnounceEvent event{AnnounceEvent::none};
        std::uint32_t numwant{50};
        std::uint32_t key{0};
        bool compact{true};
        bool no_peer_id{true};
        std::optional<std::string> ipv6;
        std::optional<std::string> trackerId;
    };


    struct AnnounceResponse 
    {
        std::uint32_t interval{1800};
        std::optional<std::uint32_t> minInterval;
        std::uint32_t complete{0}; // seeders
        std::uint32_t incomplete{0}; // leechers
        std::vector<PeerAddr> peers;
        std::optional<std::string> warning;
        std::optional<std::string> trackerId;
    };


    struct ScrapeStats 
    {
        std::uint32_t complete{0};
        std::uint32_t downloaded{0};
        std::uint32_t incomplete{0};
        std::optional<std::string> name;
    };
    
} // namespace bittorrent::tracker