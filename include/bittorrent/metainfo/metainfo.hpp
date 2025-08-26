#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <filesystem>

#include "../../bencode/bencode.hpp"

class FileEntry {
public:
    std::filesystem::path path; // relative path in torrent
    uint64_t length{0};         // file length in bytes
    uint64_t offset{0};         // global offset from start of torrent payload
};

class InfoDictionary {
public:
    std::string name;
    std::vector<FileEntry> files;                       // "files" or single entry
    uint32_t pieceLength{0};
    std::vector<std::array<uint8_t,20>> pieces;

    // optional: raw slice for info-hash (set by fromTorrent)
    std::string_view rawSlice;
};

class Metainfo {
public:

    static Metainfo fromTorrent(std::string_view data);
    static Metainfo fromMagnet(const std::string& uri);

    const std::vector<std::array<uint8_t,20>>& pieces() const { return info.pieces; }
    uint32_t pieceLength() const { return info.pieceLength; }


    InfoDictionary info;
    std::vector<std::string> announceList;

    bool isSingleFile() const { return info.files.size() == 1; }

    uint64_t totalLength() const {
        uint64_t total = 0;
        for (auto& f : info.files) total += f.length;
        return total;
    }

    std::array<uint8_t,20> infoHash() const { return infoHash_; }

private:
    std::array<uint8_t,20> infoHash_{};
};
