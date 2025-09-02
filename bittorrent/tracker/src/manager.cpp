#include <chrono>
#include <regex>
#include "../include/manager.hpp"


namespace bittorrent::tracker {

  static Scheme detectScheme(const std::string& url) {
    if (url.rfind("udp://", 0) == 0) return Scheme::udp;
    if (url.rfind("https://", 0) == 0) return Scheme::https;
    return Scheme::http;
  }

  static std::string makeScrapeUrl(const std::string& announceUrl) {
    return std::regex_replace(announceUrl, std::regex("/announce(?![^/])"), "/scrape");
  }

  TrackerManager::TrackerManager(const std::vector<std::vector<std::string>>& announceList,
                                InfoHash ih, PeerID pid, std::uint16_t port,
                                std::shared_ptr<IHttpClient> httpClient)
    : infoHash_(ih), peerId_(pid), port_(port) 
  {
    if (!httpClient) httpClient = makeCurlClient();
    http_ = std::make_shared<HttpTracker>(std::move(httpClient));
    udp_  = std::make_shared<UdpTracker>();

    tiers_.reserve(announceList.size());
    for (auto const& tierUrls : announceList) {
      TrackerTier tier;
      for (auto const& u : tierUrls) {
        TrackerEndpoint ep; ep.url = u; ep.scheme = detectScheme(u);
        tier.endpoints.push_back(std::move(ep));
      }
      tiers_.push_back(std::move(tier));
    }
  }

  TrackerManager::~TrackerManager() { stop(); }

  void TrackerManager::start() {
    if (running_.exchange(true)) return;
    worker_ = std::thread([this]{ workerLoop(); });
  }

  void TrackerManager::stop() {
    if (!running_.exchange(false)) return;
    if (worker_.joinable()) worker_.join();
  }

  void TrackerManager::onStats(std::uint64_t up, std::uint64_t down, std::uint64_t left) {
    std::scoped_lock lk(statsMu_); uploaded_ = up; downloaded_ = down; left_ = left;
  }

  AnnounceRequest TrackerManager::makeReq(AnnounceEvent ev, std::uint32_t numwant) const {
    AnnounceRequest r; r.infoHash = infoHash_; r.peerId = peerId_; r.port = port_; r.event = ev; r.numwant = numwant; r.compact = true; r.no_peer_id = true; return r;
  }

  ITrackerClient& TrackerManager::clientFor(const TrackerEndpoint& ep) {
    switch (ep.scheme) { case Scheme::udp: return *udp_; case Scheme::https: [[fallthrough]]; case Scheme::http: return *http_; }
    return *http_;
  }

  void TrackerManager::deliverPeers(const std::vector<PeerAddr>& peers) {
    if (peersCb_) { peersCb_(peers); }
  }

  void TrackerManager::tryOneTier(TrackerTier& tier, AnnounceEvent ev, std::uint32_t numwant) {
    auto now = std::chrono::steady_clock::now();
    const auto startIdx = tier.currentIndex;

    for (std::size_t tries = 0; tries < tier.endpoints.size(); ++tries) {
      auto& ep = tier.current();
      if (!ep.canAnnounceNow(now)) { tier.rotate(); continue; }

      AnnounceRequest req;
      {
        std::scoped_lock lk(statsMu_);
        req = makeReq(ev, numwant);
        req.uploaded = uploaded_; req.downloaded = downloaded_; req.left = left_;
        if (ep.trackerId) req.trackerId = ep.trackerId;
      }

      auto& client = clientFor(ep);
      auto res = client.announce(req, ep.url);
      if (res.has_value()) {
        auto& a = res.get();
        ep.recordSuccess(a.minInterval.value_or(a.interval), a.minInterval);
        if (a.trackerId) ep.trackerId = a.trackerId;
        if (!a.peers.empty()) {
          {
            std::scoped_lock lk(peersMu_);
            for (auto& p : a.peers) pendingPeers_.push_back(p);
          }

          deliverPeers(a.peers);
        }
        return; // success, stop trying endpoints in this tier
      } else {
        ep.recordFailure();
        tier.rotate();
      }

      if (tier.currentIndex == startIdx) break; // full loop
    }
  }

  void TrackerManager::workerLoop() {
    using namespace std::chrono_literals;

    while (running_) {
      auto now = std::chrono::steady_clock::now();
      bool didWork = false;

      for (auto& tier : tiers_) {
        if (!running_) break;
        if (tier.anyAvailable(now)) { tryOneTier(tier, AnnounceEvent::none, 50); didWork = true; break; }
      }

      if (!running_) break;

      std::chrono::steady_clock::time_point earliest = now + 5s; bool haveEarliest=false;
      for (auto& tier : tiers_) {
        for (auto& ep : tier.endpoints) {
          if (ep.disabled) continue;
          if (ep.nextAllowed.time_since_epoch().count() == 0) { earliest = now + 1s; haveEarliest=true; continue; }
          if (!haveEarliest || ep.nextAllowed < earliest) { earliest = ep.nextAllowed; haveEarliest = true; }
        }
      }

      using namespace std::chrono;

      auto sleepDur = 1s;
      if (haveEarliest) {
        auto now = steady_clock::now();
        auto delta = earliest - now; // steady_clock::duration
        sleepDur = (delta > 0ns) ? duration_cast<seconds>(delta) : 1s;
      }

      std::this_thread::sleep_for(sleepDur); 
    }
  }

  std::vector<PeerAddr> TrackerManager::drainNewPeers() {
    std::scoped_lock lk(peersMu_); std::vector<PeerAddr> out; out.swap(pendingPeers_); return out;
  }

  void TrackerManager::announce(AnnounceEvent ev, std::uint32_t numwant) {
    for (auto& tier : tiers_) { if (tier.anyAvailable(std::chrono::steady_clock::now())) { tryOneTier(tier, ev, numwant); return; } }
  }

  void TrackerManager::setPeersCallback(PeersCallback cb) { peersCb_ = std::move(cb); }

} // namespace bittorrent::tracker
