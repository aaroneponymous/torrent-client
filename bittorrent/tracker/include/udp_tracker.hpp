#pragma once
#include "iclient.hpp"


namespace bittorrent::tracker {


    class UdpTracker : public ITrackerClient {
    public:

        Expected<AnnounceResponse> announce(const AnnounceRequest& req, const std::string& announceUrl) override {
            (void)req; (void)announceUrl;
            return Expected<AnnounceResponse>::failure("UDP tracker not implemented yet");
        }

        Expected<std::map<InfoHash, ScrapeStats>> scrape(const std::vector<InfoHash>&, const std::string&) override {
            return Expected<std::map<InfoHash, ScrapeStats>>::failure("UDP scrape not implemented yet");
        }
    };


} // namespace bittorrent::tracker