#include "../metainfo.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>

// helper: load entire file into string
std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("failed to open " + path);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

// helper: convert 20-byte hash to hex
std::string hexHash(const std::array<uint8_t,20>& h) {
    std::ostringstream oss;
    for (uint8_t b : h) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    }
    return oss.str();
}

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            std::cerr << "Usage: " << argv[0] << " <torrent-file> | <magnet-uri>\n";
            return 1;
        }

        std::string arg1 = argv[1];

        if (arg1.rfind("magnet:?", 0) == 0) {
            // Magnet link
            auto meta = Metainfo::fromMagnet(arg1);

            std::cout << "=== Magnet Metadata ===\n";
            std::cout << "InfoHash: " << hexHash(meta.infoHash()) << "\n";
            if (!meta.info.name.empty())
                std::cout << "Name: " << meta.info.name << "\n";
            if (!meta.announceList.empty()) {
                std::cout << "Trackers:\n";
                for (auto& url : meta.announceList) {
                    std::cout << "  " << url << "\n";
                }
            }
        } else {
            // Torrent file
            std::string fileData = readFile(arg1);
            auto meta = Metainfo::fromTorrent(fileData);

            std::cout << "=== Torrent Metadata ===\n";
            if (!meta.announceList.empty()) {
                std::cout << "Trackers:\n";
                for (auto& url : meta.announceList) {
                    std::cout << "  " << url << "\n";
                }
            }

            std::cout << "InfoHash: " << hexHash(meta.infoHash()) << "\n";
            std::cout << "Name: " << meta.info.name << "\n";
            std::cout << "Piece length: " << meta.pieceLength() << "\n";
            std::cout << "Total length: " << meta.totalLength() << "\n";
            std::cout << "Pieces count: " << meta.pieces().size() << "\n";

            std::cout << "Files:\n";
            for (auto& f : meta.info.files) {
                std::cout << "  " << f.path << " (" << f.length
                          << " bytes, offset=" << f.offset << ")\n";
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
