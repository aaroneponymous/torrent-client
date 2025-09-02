#pragma once
#include <string>
#include <string_view>
#include <memory>
#include "iclient.hpp"
#include "http_client.hpp"


namespace bittorrent::tracker {


    struct HttpTrackerConfig 
    {
        int connectTimeoutSec{8};
        int transferTimeoutSec{10};
        bool followRedirects{true};
    };


    class HttpTracker : public ITrackerClient 
    {
    public:
        explicit HttpTracker(std::shared_ptr<IHttpClient> http, HttpTrackerConfig cfg = {});
        ~HttpTracker() override = default;


        Expected<AnnounceResponse> announce(const AnnounceRequest& req, const std::string& announceUrl) override;
        Expected<std::map<InfoHash, ScrapeStats>> scrape(const std::vector<InfoHash>& hashes,
        const std::string& scrapeUrl) override;

    private:
        std::shared_ptr<IHttpClient> http_;
        HttpTrackerConfig cfg_{};
        std::string buildAnnounceUrl(const std::string& base, const AnnounceRequest& req) const;
        static std::string percentEncode(std::string_view raw);
        static std::string percentEncodeBinary(const unsigned char* data, std::size_t len);
        Expected<AnnounceResponse> parseAnnounceBody(const std::string& body) const;
        Expected<std::map<InfoHash, ScrapeStats>> parseScrapeBody(const std::string& body) const;

    };


    // Factory (implemented in http_client_curl.cpp)
    std::shared_ptr<IHttpClient> makeCurlClient();


} // namespace bittorrent::tracker