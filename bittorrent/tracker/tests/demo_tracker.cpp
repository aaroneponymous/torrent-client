#include <iostream>
#include <fstream>
#include "../../metainfo/metainfo.hpp"
#include "../include/manager.hpp"
#include "../include/http_client.hpp"

using namespace bittorrent::tracker;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: demo_tracker <file.torrent>\n";
        return 1;
    }

    std::string path = argv[1];

    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::cerr << "Cannot open " << path << "\n";
        return 1;
    }

    std::string data((std::istreambuf_iterator<char>(f)), {});
    auto meta = Metainfo::fromTorrent(data);

    std::cout << "Announce list:\n";
    for (size_t i = 0; i < meta.announceList.size(); ++i) {
        std::cout << "  Tier " << i << ":\n";
        for (auto& url : meta.announceList[i]) {
            std::cout << "    " << url << "\n";
        }
    }


    PeerID pid{};
    for (int i = 0; i < 20; ++i) pid.bytes[i] = static_cast<uint8_t>(i);

    auto http = makeCurlClient();
    
    InfoHash hash;
    hash.bytes = meta.infoHash();
    std::vector<std::vector<std::string>> announceList(meta.announceList);


    
    TrackerManager mgr(announceList, hash, pid, 6881, http);
    mgr.start();
    mgr.onStats(0, 0, meta.totalLength());

    mgr.announce(AnnounceEvent::started, 30);

    // give some time for background thread
    std::this_thread::sleep_for(std::chrono::seconds(5));

    auto peers = mgr.drainNewPeers();
    std::cout << "Peers:\n";
    for (auto& p : peers) {
        std::cout << "  " << p.ip << ":" << p.port << "\n";
    }


    mgr.stop();
}
