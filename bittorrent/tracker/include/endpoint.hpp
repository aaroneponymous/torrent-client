#pragma once
#include <chrono>
#include <string>
#include <vector>
#include <optional>
#include "types.hpp"


namespace bittorrent::tracker {


    struct TrackerEndpoint 
    {
        std::string url; // full announce URL or udp URL
        Scheme scheme{Scheme::http};
        std::chrono::steady_clock::time_point lastAnnounce{};
        std::chrono::steady_clock::time_point nextAllowed{};
        std::uint32_t failureCount{0};
        bool disabled{false};
        std::optional<std::string> trackerId;


        void recordSuccess(std::uint32_t interval, std::optional<std::uint32_t> minInterval);
        void recordFailure();
        bool canAnnounceNow(std::chrono::steady_clock::time_point now) const;
    };


    struct TrackerTier 
    {
        std::vector<TrackerEndpoint> endpoints;
        std::size_t currentIndex{0};


        TrackerEndpoint& current();
        void rotate();
        bool anyAvailable(std::chrono::steady_clock::time_point now) const;
    };


} // namespace bittorrent::tracker