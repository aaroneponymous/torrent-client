#include <cstring>
#include <sstream>
#include <vector>
#include <iomanip>
#include "../include/http_tracker.hpp"
#include "../include/compact_peer_codec.hpp"
#include "../../bencode/bencode.hpp"

namespace bittorrent::tracker {

    HttpTracker::HttpTracker(std::shared_ptr<IHttpClient> http, HttpTrackerConfig cfg)
        : http_(std::move(http)), cfg_(cfg) {}


    std::string HttpTracker::percentEncode(std::string_view raw) 
    {
        std::ostringstream oss;
        for (unsigned char c : raw) {
            if ((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~') oss<<c; 
            else {
                oss<<'%'<<std::uppercase<<std::hex<<std::setw(2)<<std::setfill('0')<<(int)c<<std::nouppercase<<std::dec;}
            }
        return oss.str();
    }


    std::string HttpTracker::percentEncodeBinary(const unsigned char* data, std::size_t len) {
        return percentEncode(std::string_view(reinterpret_cast<const char*>(data), len));
    }


    std::string HttpTracker::buildAnnounceUrl(const std::string& base, const AnnounceRequest& req) const 
    {
        std::ostringstream url; url << base; if (base.find('?')==std::string::npos) url<<'?'; else url<<'&';

        url << "info_hash=" << percentEncodeBinary(req.infoHash.bytes.data(), req.infoHash.bytes.size());
        url << "&peer_id=" << percentEncodeBinary(req.peerId.bytes.data(), req.peerId.bytes.size());
        url << "&port=" << req.port;
        url << "&uploaded=" << req.uploaded;
        url << "&downloaded=" << req.downloaded;
        url << "&left=" << req.left;

        if (req.event != AnnounceEvent::none) {
            const char* e = (req.event == AnnounceEvent::started) ? "started" : (req.event == AnnounceEvent::completed ? "completed" : "stopped");
            url << "&event=" << e;
        }

        url << "&compact=" << (req.compact ? 1 : 0);
        url << "&numwant=" << req.numwant;
        url << "&key=" << req.key;

        if (req.no_peer_id) url << "&no_peer_id=1";
        if (req.ipv6) url << "&ipv6=" << percentEncode(*req.ipv6);
        if (req.trackerId) url << "&trackerid=" << percentEncode(*req.trackerId);

        return url.str();
    }


    Expected<AnnounceResponse> HttpTracker::parseAnnounceBody(const std::string& body) const 
    {
        using namespace bencode;

        BencodeValue root = BencodeParser::parse(std::string_view(body));

        if (!root.isDict()) return Expected<AnnounceResponse>::failure("announce body not a dict");

        const auto& dict = root.asDict();

        if (auto it = dict.find("failure reason"); it!=dict.end() && it->second.isString()) {
            return Expected<AnnounceResponse>::failure(it->second.asString());
        }


        AnnounceResponse resp;

        if (auto it = dict.find("interval"); it!=dict.end() && it->second.isInt()) resp.interval = (std::uint32_t)it->second.asInt();
        if (auto it = dict.find("min interval"); it!=dict.end() && it->second.isInt()) resp.minInterval = (std::uint32_t)it->second.asInt();
        if (auto it = dict.find("complete"); it!=dict.end() && it->second.isInt()) resp.complete = (std::uint32_t)it->second.asInt();
        if (auto it = dict.find("incomplete"); it!=dict.end() && it->second.isInt()) resp.incomplete = (std::uint32_t)it->second.asInt();
        if (auto it = dict.find("warning message"); it!=dict.end() && it->second.isString()) resp.warning = it->second.asString();
        if (auto it = dict.find("tracker id"); it!=dict.end() && it->second.isString()) resp.trackerId = it->second.asString();


        if (auto it = dict.find("peers"); it!=dict.end()) {

            if (it->second.isString()) {
                const auto& s = it->second.asString();
                auto v4 = CompactPeerCodec::parseIPv4(std::string_view(s));
                resp.peers.insert(resp.peers.end(), v4.begin(), v4.end());

            } else if (it->second.isList()) {
                for (auto const& item : it->second.asList()) {
                    if (!item.isDict()) continue; const auto& d = item.asDict(); PeerAddr pa;
                    if (auto ipIt = d.find("ip"); ipIt!=d.end() && ipIt->second.isString()) pa.ip = ipIt->second.asString();
                    if (auto pIt = d.find("port"); pIt!=d.end() && pIt->second.isInt()) pa.port = (std::uint16_t)pIt->second.asInt();
                    resp.peers.push_back(std::move(pa));
                }
            }
        }

        if (auto it = dict.find("peers6"); it!=dict.end() && it->second.isString()) {
            const auto& s = it->second.asString(); auto v6 = CompactPeerCodec::parseIPv6(std::string_view(s));
            resp.peers.insert(resp.peers.end(), v6.begin(), v6.end());
        }

        return Expected<AnnounceResponse>::success(std::move(resp));
    }


    Expected<std::map<InfoHash, ScrapeStats>> HttpTracker::parseScrapeBody(const std::string& body) const 
    {
        using namespace bencode;

        BencodeValue root = BencodeParser::parse(std::string_view(body));
        if (!root.isDict()) return Expected<std::map<InfoHash, ScrapeStats>>::failure("scrape body not a dict");

        const auto& d = root.asDict();
        auto filesIt = d.find("files");

        if (filesIt == d.end() || !filesIt->second.isDict())
            return Expected<std::map<InfoHash, ScrapeStats>>::failure("scrape has no files dict");

        std::map<InfoHash, ScrapeStats> out;
        for (auto const& [k, v] : filesIt->second.asDict()) {
            if (!v.isDict()) continue;

            ScrapeStats s{}; const auto& sd = v.asDict();
            if (auto it=sd.find("complete");   it!=sd.end() && it->second.isInt()) s.complete   = (std::uint32_t)it->second.asInt();
            if (auto it=sd.find("downloaded"); it!=sd.end() && it->second.isInt()) s.downloaded = (std::uint32_t)it->second.asInt();
            if (auto it=sd.find("incomplete"); it!=sd.end() && it->second.isInt()) s.incomplete = (std::uint32_t)it->second.asInt();
            if (auto it=sd.find("name");       it!=sd.end() && it->second.isString()) s.name = it->second.asString();
            InfoHash ih{}; if (k.size()==20) std::memcpy(ih.bytes.data(), k.data(), 20); out.emplace(ih, s);
        }

        return Expected<std::map<InfoHash, ScrapeStats>>::success(std::move(out));
    }

    Expected<AnnounceResponse> HttpTracker::announce(const AnnounceRequest& req, const std::string& announceUrl) 
    {
        auto url = buildAnnounceUrl(announceUrl, req);
        auto resp = http_->get(url, cfg_.connectTimeoutSec, cfg_.transferTimeoutSec, cfg_.followRedirects);

        if (!resp.has_value()) return Expected<AnnounceResponse>::failure(resp.error->message);
        return parseAnnounceBody(resp.get().body);
    }

    Expected<std::map<InfoHash, ScrapeStats>> HttpTracker::scrape(const std::vector<InfoHash>&, const std::string& scrapeUrl) 
    {
        auto resp = http_->get(scrapeUrl, cfg_.connectTimeoutSec, cfg_.transferTimeoutSec, cfg_.followRedirects);

        if (!resp.has_value()) return Expected<std::map<InfoHash, ScrapeStats>>::failure(resp.error->message);
        return parseScrapeBody(resp.get().body);
    }

} // namespace bittorrent::tracker