#pragma once
#include <string>
#include <optional>
#include <cstdint>

namespace bittorrent::tracker::detail {

    /**
     * @brief Parsed pieces of a udp:// URL (we only care about host and port).
     *
     * Examples:
     *  - udp://tracker.example.org:6969/announce  -> host="tracker.example.org", port=6969
     *  - udp://tracker.example.org                -> host="tracker.example.org", port=6969 (default)
     */
    struct UdpUrlParts {
        std::string host;
        uint16_t port;
    };

    /**
     * @brief Parse a udp:// URL. Accepts missing port; defaults to 6969. Ignores path/query.
     * Returns std::nullopt on failure (wrong scheme or empty host).
     */
    std::optional<UdpUrlParts> parse_udp_url_minimal(const std::string& url);

} // namespace bittorrent::tracker::detail
