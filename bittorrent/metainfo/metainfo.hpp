#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <filesystem>
#include <optional>
#include "../bencode/bencode.hpp"


namespace bittorrent::metainfo {

    struct FileEntry 
    {
        std::filesystem::path path;
        uint64_t length{0};
        uint64_t offset{0};
    };

    struct InfoDictionary 
    {
        std::string name;
        std::vector<FileEntry> files;                       // single-file => size==1
        uint32_t pieceLength{0};
        std::vector<std::array<uint8_t,20>> pieces;
        std::string_view rawSlice;                          // exact bencoded bytes of "info"
    };

    class Metainfo 
    {
    public:
        static Metainfo fromTorrent(std::string_view data);
        static Metainfo fromMagnet(const std::string& uri); // best-effort: hash + trackers + display name

        const std::vector<std::array<uint8_t,20>>& pieces() const noexcept { return info.pieces; }
        uint32_t pieceLength() const noexcept { return info.pieceLength; }
        bool isSingleFile() const noexcept { return info.files.size() == 1; }
        
        uint64_t totalLength() const noexcept {
            uint64_t total = 0;
            for (const auto& f : info.files) total += f.length;
            return total;
        }

        std::array<uint8_t,20> infoHash() const noexcept { return infoHash_; }

        InfoDictionary info;
        std::vector<std::vector<std::string>> announceList;

    private:
        std::array<uint8_t,20> infoHash_{};
    };

}