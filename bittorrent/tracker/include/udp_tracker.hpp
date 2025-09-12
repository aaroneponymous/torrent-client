#pragma once
#include <string>
#include <vector>
#include <map>
#include <array>
#include <chrono>
#include <optional>
#include <cstdint>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <random>
#include <cstring>
#include <string>

#include "iclient.hpp"

namespace bittorrent::tracker {


    /**
     * @brief Minimal BEP-15 UDP tracker client.
     *
     * Conforms to ITrackerClient so TrackerManager can swap HTTP/UDP by scheme.
     * - Fresh UDP socket per call (robust; avoids stale FDs across processes/forks).
     * - Maintains a short-lived connection_id (60s) across calls inside this object.
     * - IPv4 peers only (compact 6-byte entries) for Phase 1; IPv6 TODO.
     *
     * Error handling:
     * - All network/protocol issues return Expected<T>::failure("...").
     * - UDP protocol error (action==3) is surfaced with the tracker’s error message text.
     */
    class UdpTracker : public ITrackerClient {
    public:
        UdpTracker() = default;
        ~UdpTracker() override = default;

        // ITrackerClient
        Expected<AnnounceResponse> announce(const AnnounceRequest& req,
                                            const std::string& announceUrl) override;

        Expected<std::map<InfoHash, ScrapeStats>>
        scrape(const std::vector<InfoHash>& hashes,
            const std::string& scrapeUrl) override;

    private:
        // ---- Connection cache (per instance) ----
        uint64_t connId_{0};
        std::chrono::steady_clock::time_point connExpiry_{};

        // ---- Core helpers (implemented in .cpp) ----
        static Expected<std::pair<sockaddr_storage, socklen_t>>
        parseUdpUrl(const std::string& url);

        static Expected<int> makeUdpSocket(int family,
                                        std::chrono::milliseconds recvTimeout);

        static Expected<std::pair<uint64_t, std::chrono::steady_clock::time_point>>
        connectAndGetConnId(const sockaddr_storage& addr, socklen_t addrlen, int sock,
                            std::chrono::milliseconds timeoutPerAttempt,
                            int maxAttempts, std::chrono::milliseconds backoffStart);

        Expected<AnnounceResponse>
        doAnnounce(const sockaddr_storage& addr, socklen_t addrlen, int sock,
                const AnnounceRequest& req,
                std::chrono::milliseconds timeoutPerAttempt,
                int maxAttempts, std::chrono::milliseconds backoffStart);

        Expected<std::map<InfoHash, ScrapeStats>>
        doScrape(const sockaddr_storage& addr, socklen_t addrlen, int sock,
                const std::vector<InfoHash>& hashes,
                std::chrono::milliseconds timeoutPerAttempt,
                int maxAttempts, std::chrono::milliseconds backoffStart);

        // ---- Binary packing helpers (network byte order, big-endian) ----
        static inline void put_u16(std::vector<uint8_t>& b, uint16_t v);
        static inline void put_u32(std::vector<uint8_t>& b, uint32_t v);
        static inline void put_u64(std::vector<uint8_t>& b, uint64_t v);
        static inline uint16_t get_u16(const uint8_t* p);
        static inline uint32_t get_u32(const uint8_t* p);
        static inline uint64_t get_u64(const uint8_t* p);

        static uint32_t rand_u32();

        // ---- Small tuning knobs (constants) ----
        static constexpr std::chrono::seconds kConnTtl{60};
        static constexpr std::chrono::milliseconds kTimeout{1500};
        static constexpr int kMaxAttempts = 8;
        static constexpr std::chrono::milliseconds kBackoff{1500};
    };

} // namespace bittorrent::tracker
