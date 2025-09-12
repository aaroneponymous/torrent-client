#include "udp_url.hpp"


#include "udp_url.hpp"
#include <string>
#include <string_view>
#include <cctype>

namespace bittorrent::tracker::detail {

    static inline bool has_prefix(const std::string& s, const char* pfx) {
        const size_t n = std::char_traits<char>::length(pfx);
        return s.size() >= n && s.compare(0, n, pfx) == 0;
    }

    std::optional<UdpUrlParts> parse_udp_url_minimal(const std::string& url)
    {
        if (!has_prefix(url, "udp://")) return std::nullopt;

        // Strip scheme
        const std::string_view rest(url.c_str() + 6, url.size() - 6);

        // host[:port][/...]
        size_t slash = rest.find('/');
        std::string_view hostport = (slash == std::string_view::npos) ? rest : rest.substr(0, slash);

        if (hostport.empty()) return std::nullopt;

        // NOTE: This minimal parser does not implement bracketed IPv6 literals.
        size_t colon = hostport.rfind(':');
        std::string host;
        uint16_t port = 6969; // common default for trackers

        if (colon != std::string_view::npos) {
            host = std::string(hostport.substr(0, colon));
            std::string_view port_sv = hostport.substr(colon + 1);
            if (!port_sv.empty()) {
                unsigned long p = 0;
                for (char ch : port_sv) {
                    if (!std::isdigit(static_cast<unsigned char>(ch))) { p = 0; break; }
                    p = p * 10 + (ch - '0');
                    if (p > 65535) { p = 0; break; }
                }
                if (p == 0) return std::nullopt;
                port = static_cast<uint16_t>(p);
            }
        } else {
            host = std::string(hostport);
        }

        if (host.empty()) return std::nullopt;
        return UdpUrlParts{std::move(host), port};
    }

} // namespace bittorrent::tracker::detail
