#pragma once
#include <map>
#include <vector>
#include "types.hpp"
#include "expected.hpp"


namespace bittorrent::tracker {


    struct ITrackerClient 
    {
        virtual ~ITrackerClient() = default;
        virtual Expected<AnnounceResponse> announce(const AnnounceRequest& req, const std::string& announceUrl) = 0;
        virtual Expected<std::map<InfoHash, ScrapeStats>> scrape(const std::vector<InfoHash>& hashes,
        const std::string& scrapeUrl) = 0;
    };


} // namespace bittorrent::tracker