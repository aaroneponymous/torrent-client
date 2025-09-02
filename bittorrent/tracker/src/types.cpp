#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>
#include "../include/types.hpp"


namespace bittorrent::tracker {


    std::string InfoHash::toHex() const {
        std::ostringstream oss;
        for (auto b : bytes) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
        return oss.str();
    }


} // namespace bittorrent::tracker