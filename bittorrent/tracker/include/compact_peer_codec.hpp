#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "types.hpp"


namespace bittorrent::tracker {

    struct CompactPeerCodec 
    {
        static std::vector<PeerAddr> parseIPv4(std::string_view raw);
        static std::vector<PeerAddr> parseIPv6(std::string_view raw);
    };

} // namespace bittorrent::tracker