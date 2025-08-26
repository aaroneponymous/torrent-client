#include "metainfo.hpp"
#include "../sha/sha1.hpp"

#include <cstring>
#include <sstream>
#include <stdexcept>
#include <algorithm>

// Decode base32 string (RFC 4648) into raw bytes
static std::vector<uint8_t> base32Decode(const std::string& input) {
    static const int8_t table[256] = {
        /* 0-31 */ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        /* ' ' - '/' */ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        /* '0'-'9' */    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        /* ':'-'@' */    -1,-1,-1,-1,-1,-1,-1,
        /* 'A'-'Z' */     0, 1, 2, 3, 4, 5, 6, 7,
                          8, 9,10,11,12,13,14,15,
                         16,17,18,19,20,21,22,23,
                         24,25,
        /* '[' - '`' */ -1,-1,-1,-1,-1,-1,
        /* 'a'-'z' */    26,27,28,29,30,31, /* rest -1 */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,
        /* rest all -1 */ 
    };

    std::vector<uint8_t> output;
    int buffer = 0, bitsLeft = 0;
    for (char c : input) {
        int8_t val = (c >= 0 && c < 127) ? table[static_cast<int>(c)] : -1;
        if (val == -1) continue; // skip invalid chars (padding etc.)

        buffer <<= 5;
        buffer |= val & 31;
        bitsLeft += 5;
        if (bitsLeft >= 8) {
            output.push_back((buffer >> (bitsLeft - 8)) & 0xFF);
            bitsLeft -= 8;
        }
    }
    return output;
}


// 40-char SHA1 hex string into 20 raw bytes
static std::array<uint8_t,20> hexToRaw(const std::string& hex) {
    if (hex.size() != 40) {
        throw std::runtime_error("SHA1 hex string must be exactly 40 chars");
    }
    std::array<uint8_t,20> out{};
    for (size_t i = 0; i < 20; i++) {
        unsigned int byte;
        std::stringstream ss(hex.substr(i*2, 2));
        ss >> std::hex >> byte;
        out[i] = static_cast<uint8_t>(byte);
    }
    return out;
}


// binary blob into 20-byte hashes
static std::vector<std::array<uint8_t,20>> splitPieces(const std::string& blob) {
    if (blob.size() % 20 != 0) {
        throw std::runtime_error("pieces field length not multiple of 20");
    }
    std::vector<std::array<uint8_t,20>> out;
    for (size_t i=0; i<blob.size(); i+=20) {
        std::array<uint8_t,20> sha{};
        std::memcpy(sha.data(), blob.data()+i, 20);
        out.push_back(sha);
    }
    return out;
}


static std::string urlDecode(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') {
            result.push_back(' ');
        } else if (s[i] == '%' && i + 2 < s.size()) {
            unsigned int byte;
            std::stringstream ss;
            ss << std::hex << s.substr(i+1, 2);
            ss >> byte;
            result.push_back(static_cast<char>(byte));
            i += 2;
        } else {
            result.push_back(s[i]);
        }
    }
    return result;
}


Metainfo Metainfo::fromTorrent(std::string_view data) {
    using namespace bencode;

    BencodeValue root = BencodeParser::parse(data);
    if (!root.isDict()) throw std::runtime_error("torrent root is not dict");

    const auto& dict = root.asDict();
    Metainfo meta;

    // --- announce ---
    if (auto it = dict.find("announce"); it != dict.end() && it->second.isString()) {
        meta.announceList.push_back(it->second.asString());
    }

    if (auto it = dict.find("announce-list"); it != dict.end() && it->second.isList()) {
        for (auto& tier : it->second.asList()) {
            if (tier.isList()) {
                for (auto& url : tier.asList()) {
                    if (url.isString()) meta.announceList.push_back(url.asString());
                }
            }
        }
    }
    
    auto itInfo = dict.find("info");
    if (itInfo == dict.end() || !itInfo->second.isDict())
        throw std::runtime_error("missing info dict");

    const auto& infoDict = itInfo->second.asDict();
    InfoDictionary info;

    if (auto it = infoDict.find("name"); it != infoDict.end() && it->second.isString()) {
        info.name = it->second.asString();
    }

    if (auto it = infoDict.find("piece length"); it != infoDict.end() && it->second.isInt()) {
        info.pieceLength = static_cast<uint32_t>(it->second.asInt());
    }

    if (auto it = infoDict.find("pieces"); it != infoDict.end() && it->second.isString()) {
        info.pieces = splitPieces(it->second.asString());
    }
    
    if (auto it = infoDict.find("files"); it != infoDict.end() && it->second.isList()) {
        uint64_t offset = 0;
        for (auto& f : it->second.asList()) {
            const auto& fd = f.asDict();
            uint64_t len = fd.at("length").asInt();
            std::filesystem::path path;
            for (auto& seg : fd.at("path").asList()) {
                path /= seg.asString();
            }
            info.files.push_back({path, len, offset});
            offset += len;
        }
    } else if (auto it = infoDict.find("length"); it != infoDict.end() && it->second.isInt()) {
        uint64_t len = it->second.asInt();
        info.files.push_back({info.name, len, 0});
    }

    meta.info = std::move(info);

    /**
     * Info-Hash Computation (SHA-1)
     * @note: BencodeParser doesn't return slice
     *        Re-encoding the "info" dict and hasing that
     * @todo: replace with raw slice hashing
     */


    std::string encodedInfo = bencode::BencodeParser::encode(itInfo->second);

    SHA1 sha;
    sha.update(encodedInfo);

    /**
     * Info-Hash SHA-1: Return Type
     * @note: sha.final() -> returns hex string 20-bytes digest as 40-bytes hex string
     * @note: Conversion to 40-bytes hex string and then to raw_bytes to store in infoHash_
     * @todo: Implement a .rawFianl() method in sha1.hpp
     */

    std::string hexDigest = sha.final();
    meta.infoHash_ = hexToRaw(hexDigest);

    return meta;
}

Metainfo Metainfo::fromMagnet(const std::string& uri) {
    Metainfo meta;

    // Parse magnet:?xt=urn:btih:<hash>&dn=<name>&tr=<tracker>
    auto qpos = uri.find('?');
    if (qpos == std::string::npos) throw std::runtime_error("invalid magnet URI");

    std::string query = uri.substr(qpos+1);
    std::istringstream iss(query);
    std::string token;

    while (std::getline(iss, token, '&')) {
        auto eq = token.find('=');
        if (eq == std::string::npos) continue;

        std::string key = token.substr(0, eq);
        std::string val = token.substr(eq+1);
        if (key == "xt" && val.rfind("urn:btih:", 0) == 0) {
            std::string hash = val.substr(9); // after "urn:btih:"
            std::array<uint8_t,20> h{};

            if (hash.size() == 40) {
                // --- hex form (40 chars = 20 bytes) ---
                for (size_t i = 0; i < 20; i++) {
                    unsigned int byte;
                    std::stringstream ss(hash.substr(i*2, 2));
                    ss >> std::hex >> byte;
                    h[i] = static_cast<uint8_t>(byte);
                }
        } else if (hash.size() == 32) {
            // --- base32 form (32 chars = 20 bytes) ---
            auto raw = base32Decode(hash);     // use helper
            if (raw.size() != 20) {
                throw std::runtime_error("invalid base32 info-hash size");
            }
            std::copy(raw.begin(), raw.end(), h.begin());
        } else {
            throw std::runtime_error("invalid btih length (expected 40 hex or 32 base32)");
        }

        meta.infoHash_ = h;
    }

        else if (key == "dn") {
            meta.info.name = urlDecode(val);
        } else if (key == "tr") {
            meta.announceList.push_back(urlDecode(val));
        }
    }

    return meta;
}
