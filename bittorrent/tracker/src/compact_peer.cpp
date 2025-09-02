#include <arpa/inet.h>
#include <cstring>
#include "../include/compact_peer_codec.hpp"

namespace bittorrent::tracker {

    static std::vector<PeerAddr> parseCompact(std::string_view raw, int elemBytes) {
        std::vector<PeerAddr> out;

        if (elemBytes != 6 && elemBytes != 18) {
            return out;
        }

        const std::size_t n = raw.size() / elemBytes;
        out.reserve(n);

        const unsigned char* p = reinterpret_cast<const unsigned char*>(raw.data());

        for (std::size_t i = 0; i < n; ++i) {
            PeerAddr pa;
            char buf[INET6_ADDRSTRLEN] = {0};

            if (elemBytes == 6) {
                // IPv4: 4 bytes address + 2 bytes port
                if (!inet_ntop(AF_INET, p + i * 6, buf, sizeof(buf))) {
                    continue;
                }
                pa.ip = buf;

                std::uint16_t port;
                std::memcpy(&port, p + i * 6 + 4, 2);
                pa.port = ntohs(port);
            } else {
                // IPv6: 16 bytes address + 2 bytes port
                if (!inet_ntop(AF_INET6, p + i * 18, buf, sizeof(buf))) {
                    continue;
                }
                pa.ip = buf;

                std::uint16_t port;
                std::memcpy(&port, p + i * 18 + 16, 2);
                pa.port = ntohs(port);
            }

            out.push_back(std::move(pa));
        }

        return out;
    }

    std::vector<PeerAddr> CompactPeerCodec::parseIPv4(std::string_view raw) {
        if (raw.size() % 6 != 0) {
            return {};
        }
        return parseCompact(raw, 6);
    }

    std::vector<PeerAddr> CompactPeerCodec::parseIPv6(std::string_view raw) {
        if (raw.size() % 18 != 0) {
            return {};
        }
        return parseCompact(raw, 18);
    }

} // namespace bittorrent::tracker
