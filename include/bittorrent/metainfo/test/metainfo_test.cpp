// metainfo_test.cpp
// Simple CLI tester for Metainfo parsing (torrent file or magnet URI).
// Build example (adjust include paths as needed):
//   g++ -std=c++17 -O2 -I.. metainfo_test.cpp -o metainfo_test
//
// Usage:
//   ./metainfo_test <path/to/file.torrent>
//   ./metainfo_test <magnet-uri>
//   ./metainfo_test <path/to/file.torrent> <expected_infohash_hex_40chars>

#include "../metainfo.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("failed to open " + path);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

// Convert 20-byte hash to lowercase hex
static std::string hexHash(const std::array<uint8_t,20>& h) {
    std::ostringstream oss;
    oss << std::hex << std::nouppercase;
    for (uint8_t b : h) {
        oss << std::setw(2) << std::setfill('0') << static_cast<unsigned>(b);
    }
    return oss.str();
}

// Check if a string is exactly 40 hex chars
static bool isHex40(const std::string& s) {
    if (s.size() != 40) return false;
    for (unsigned char c : s) {
        if (!std::isxdigit(c)) return false;
    }
    return true;
}

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            std::cerr << "Usage: " << argv[0]
                      << " <torrent-file> | <magnet-uri> [expected_infohash_hex]\n";
            return 1;
        }

        std::string arg1 = argv[1];

        auto printHashIfAny = [&](const std::array<uint8_t,20>& h) {
            bool all_zero = std::all_of(h.begin(), h.end(),
                                        [](uint8_t b){ return b == 0; });
            std::cout << "InfoHash: " << (all_zero ? "(none)" : hexHash(h)) << "\n";
        };

        if (arg1.rfind("magnet:?", 0) == 0) {
            // Magnet link
            Metainfo meta = Metainfo::fromMagnet(arg1);

            std::cout << "=== Magnet Metadata ===\n";
            printHashIfAny(meta.infoHash());

            if (!meta.info.name.empty())
                std::cout << "Name: " << meta.info.name << "\n";

            if (!meta.announceList.empty()) {
                std::cout << "Trackers:\n";
                for (const auto& url : meta.announceList) {
                    std::cout << "  " << url << "\n";
                }
            }

            if (argc >= 3 && isHex40(argv[2])) {
                std::string expect = argv[2];
                std::string got = hexHash(meta.infoHash());
                if (got != expect) {
                    std::cerr << "Expected infohash " << expect << " but got " << got << "\n";
                    return 2;
                } else {
                    std::cout << "InfoHash matches expected.\n";
                }
            }

        } else {
            // Torrent file
            std::string fileData = readFile(arg1);
            Metainfo meta = Metainfo::fromTorrent(fileData);

            std::cout << "=== Torrent Metadata ===\n";

            if (!meta.announceList.empty()) {
                std::cout << "Trackers:\n";
                for (const auto& url : meta.announceList) {
                    std::cout << "  " << url << "\n";
                }
            }

            std::cout << "Name: " << meta.info.name << "\n";
            std::cout << "Piece length: " << meta.pieceLength() << "\n";
            std::cout << "Total length: " << meta.totalLength() << "\n";
            std::cout << "Pieces count: " << meta.pieces().size();

            // Print an approximate expected number of pieces for sanity
            if (meta.pieceLength() > 0) {
                const auto expectedPieces =
                    (meta.totalLength() + meta.pieceLength() - 1) / meta.pieceLength();
                std::cout << " (expected ~" << expectedPieces << ")";
            }
            std::cout << "\n";

            std::cout << "Files:\n";
            for (const auto& f : meta.info.files) {
                std::cout << "  " << f.path
                          << " (" << f.length << " bytes"
                          << ", offset=" << f.offset << ")\n";
            }

            std::cout << "InfoHash: " << hexHash(meta.infoHash()) << "\n";

            if (argc >= 3 && isHex40(argv[2])) {
                std::string expect = argv[2];
                std::string got = hexHash(meta.infoHash());
                if (got != expect) {
                    std::cerr << "Expected infohash " << expect << " but got " << got << "\n";
                    return 2;
                } else {
                    std::cout << "InfoHash matches expected.\n";
                }
            }
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}

