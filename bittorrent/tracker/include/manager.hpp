#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include "types.hpp"
#include "endpoint.hpp"
#include "http_tracker.hpp"
#include "udp_tracker.hpp"


namespace bittorrent::tracker {


    class TrackerManager {
    public:
        using PeersCallback = std::function<void(const std::vector<PeerAddr>&)>;


        TrackerManager(const std::vector<std::vector<std::string>>& announceList,
            InfoHash ih, PeerID pid, std::uint16_t port,
            std::shared_ptr<IHttpClient> httpClient = nullptr);

        ~TrackerManager();


        void start();
        void stop();


        void onStats(std::uint64_t uploaded, std::uint64_t downloaded, std::uint64_t left);
        void announce(AnnounceEvent ev = AnnounceEvent::none, std::uint32_t numwant = 50);


        std::vector<PeerAddr> drainNewPeers();
        void setPeersCallback(PeersCallback cb);


    private:
        InfoHash infoHash_{};
        PeerID peerId_{};
        std::uint16_t port_{};


        std::uint64_t uploaded_{0}, downloaded_{0}, left_{0};
        std::mutex statsMu_;


        std::vector<TrackerTier> tiers_;


        std::shared_ptr<HttpTracker> http_;
        std::shared_ptr<UdpTracker> udp_;


        std::vector<PeerAddr> pendingPeers_;
        std::mutex peersMu_;
        PeersCallback peersCb_{};


        std::thread worker_;
        std::atomic<bool> running_{false};


        void workerLoop();
        void tryOneTier(TrackerTier& tier, AnnounceEvent ev, std::uint32_t numwant);
        ITrackerClient& clientFor(const TrackerEndpoint& ep);
        AnnounceRequest makeReq(AnnounceEvent ev, std::uint32_t numwant) const;
        void deliverPeers(const std::vector<PeerAddr>& peers);
    };


} // namespace bittorrent::tracker