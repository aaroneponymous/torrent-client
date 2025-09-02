#include <algorithm>
#include <random>
#include <stdexcept>
#include "../include/endpoint.hpp"


namespace bittorrent::tracker {


    static std::chrono::seconds clampInterval(std::uint32_t t) {
        constexpr std::uint32_t MIN = 30; // floor 30s
        constexpr std::uint32_t MAX = 3600; // cap 1h for scheduling
        if (t < MIN) t = MIN;
        if (t > MAX) t = MAX;
        return std::chrono::seconds(t);
    }


    void TrackerEndpoint::recordSuccess(std::uint32_t interval, std::optional<std::uint32_t> minInterval) {
        lastAnnounce = std::chrono::steady_clock::now();
        auto base = minInterval.value_or(interval);
        auto dur = clampInterval(base);

        // add simple jitter ±20%
        auto jitter = static_cast<int>(dur.count() * 0.2);
        std::random_device rd; std::mt19937 gen(rd());
        std::uniform_int_distribution<int> d(-jitter, jitter);

        auto next = dur + std::chrono::seconds(d(gen));
        nextAllowed = lastAnnounce + (next < std::chrono::seconds(1) ? std::chrono::seconds(1) : next);
        failureCount = 0;
    }


    void TrackerEndpoint::recordFailure() {
        ++failureCount;
        auto now = std::chrono::steady_clock::now();

        // exponential backoff: 5s * 2^failures, capped
        std::uint32_t base = 5u * (1u << std::min<std::uint32_t>(failureCount, 10));
        auto dur = clampInterval(base);

        nextAllowed = now + dur;
        if (failureCount > 7) {
            disabled = true; // optional policy
        }
    }


    bool TrackerEndpoint::canAnnounceNow(std::chrono::steady_clock::time_point now) const {
        if (disabled) return false;
        if (nextAllowed.time_since_epoch().count() == 0) return true; // never scheduled
        return now >= nextAllowed;
    }


    TrackerEndpoint& TrackerTier::current() {
        if (endpoints.empty()) throw std::runtime_error("empty tracker tier");
        if (currentIndex >= endpoints.size()) currentIndex = 0;
        return endpoints[currentIndex];
    }


    void TrackerTier::rotate() {
        if (endpoints.empty()) return;
        currentIndex = (currentIndex + 1) % endpoints.size();
    }


    bool TrackerTier::anyAvailable(std::chrono::steady_clock::time_point now) const {
        for (auto const& ep : endpoints) if (ep.canAnnounceNow(now)) return true;
        return false;
    }


} // namespace bittorrent::tracker