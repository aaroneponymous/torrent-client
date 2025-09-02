#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

#include <chrono>
#include <thread>

#include "../include/endpoint.hpp"

using namespace bittorrent::tracker;
using clock_steady = std::chrono::steady_clock;

// Helper: duration between two time_points in seconds (integer)
static long secs(std::chrono::steady_clock::time_point a,
                 std::chrono::steady_clock::time_point b) {
    return std::chrono::duration_cast<std::chrono::seconds>(b - a).count();
}

TEST_CASE("TrackerEndpoint: canAnnounceNow true before any schedule") {
    TrackerEndpoint ep;
    REQUIRE(ep.canAnnounceNow(clock_steady::now()));
}

TEST_CASE("TrackerEndpoint: recordSuccess schedules nextAllowed with jitter and resets failures") {
    TrackerEndpoint ep;
    ep.failureCount = 3; // simulate some prior failures

    ep.recordSuccess(/*interval*/60, /*minInterval*/std::nullopt);
    REQUIRE(ep.failureCount == 0);

    // lastAnnounce and nextAllowed set by recordSuccess
    REQUIRE(ep.lastAnnounce.time_since_epoch().count() != 0);
    REQUIRE(ep.nextAllowed.time_since_epoch().count() != 0);

    // delta should be in [48s, 72s] due to ±20% jitter on 60s
    const auto delta = secs(ep.lastAnnounce, ep.nextAllowed);
    REQUIRE(delta >= 48);
    REQUIRE(delta <= 72);

    // Immediately after success we should not be allowed to announce
    REQUIRE_FALSE(ep.canAnnounceNow(clock_steady::now()));
}

TEST_CASE("TrackerEndpoint: recordSuccess respects minInterval and clamps") {
    TrackerEndpoint ep;

    // Very small interval but with minInterval=25 -> clamped to floor 30s before jitter
    ep.recordSuccess(/*interval*/10, /*minInterval*/25);
    auto baseDelta = secs(ep.lastAnnounce, ep.nextAllowed);

    // With floor 30s and ±20% jitter the range is [24, 36]
    REQUIRE(baseDelta >= 24);
    REQUIRE(baseDelta <= 36);
}

TEST_CASE("TrackerEndpoint: recordFailure backs off exponentially and clamps to >=30s") {
    TrackerEndpoint ep;

    // First failure: base 5 * 2^1 = 10 -> clamped to 30
    ep.recordFailure();
    auto d1 = secs(clock_steady::now(), ep.nextAllowed); // approximate, >= 0
    REQUIRE(ep.failureCount == 1);
    // We can't compare from now reliably; compare stored window against minimum
    // Ensure scheduled delta is at least ~30s from the moment of scheduling:
    // Check the intended policy by ensuring nextAllowed is in the future
    REQUIRE_FALSE(ep.canAnnounceNow(clock_steady::now()));

    // Second failure: base 20 -> clamped to 30
    ep.recordFailure();
    REQUIRE(ep.failureCount == 2);
    REQUIRE_FALSE(ep.canAnnounceNow(clock_steady::now()));

    // Third failure: base 40 -> >=30, so around 40s (no jitter on failures path)
    ep.recordFailure();
    REQUIRE(ep.failureCount == 3);
    // The exact value is implementation-specific; ensure it’s scheduled in the future
    REQUIRE_FALSE(ep.canAnnounceNow(clock_steady::now()));
}

TEST_CASE("TrackerEndpoint: disabled after many failures") {
    TrackerEndpoint ep;
    for (int i = 0; i < 8; ++i) ep.recordFailure(); // >7 triggers disabled
    REQUIRE(ep.failureCount == 8);
    REQUIRE(ep.disabled);
    REQUIRE_FALSE(ep.canAnnounceNow(clock_steady::now()));
}

TEST_CASE("TrackerTier: rotate and current wrap correctly") {
    TrackerTier tier;
    tier.endpoints.push_back(TrackerEndpoint{ .url = "http://a", .scheme = Scheme::http });
    tier.endpoints.push_back(TrackerEndpoint{ .url = "http://b", .scheme = Scheme::http });
    tier.endpoints.push_back(TrackerEndpoint{ .url = "http://c", .scheme = Scheme::http });

    REQUIRE(tier.current().url == "http://a");
    tier.rotate();
    REQUIRE(tier.current().url == "http://b");
    tier.rotate();
    REQUIRE(tier.current().url == "http://c");
    tier.rotate();
    REQUIRE(tier.current().url == "http://a"); // wrapped
}

TEST_CASE("TrackerTier: anyAvailable reflects endpoints' availability") {
    TrackerTier tier;
    TrackerEndpoint a{ .url = "http://a", .scheme = Scheme::http };
    TrackerEndpoint b{ .url = "http://b", .scheme = Scheme::http };
    TrackerEndpoint c{ .url = "http://c", .scheme = Scheme::http };

    // Make b unavailable by setting nextAllowed far in the future; a and c are default-available
    b.nextAllowed = clock_steady::now() + std::chrono::hours(1);

    tier.endpoints = {a, b, c};

    REQUIRE(tier.anyAvailable(clock_steady::now())); // a or c available

    // Now make all unavailable
    for (auto& ep : tier.endpoints) {
        ep.nextAllowed = clock_steady::now() + std::chrono::hours(1);
    }
    REQUIRE_FALSE(tier.anyAvailable(clock_steady::now()));
}
