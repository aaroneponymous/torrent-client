#include "metainfo.hpp"
#include <stdexcept>
#include <sstream>
#include <charconv>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <openssl/sha.h>


using namespace bittorrent::metainfo;



static std::array<uint8_t,20> sha1_bytes(const void* data, size_t len) {
    std::array<uint8_t,20> out;
    SHA1(static_cast<const unsigned char*>(data), len, out.data());
    return out;
}

static const bencode::BencodeValue& expect_dict(const bencode::BencodeValue& v, const char* where) {
    if (!v.isDict()) throw std::runtime_error(std::string(where) + ": expected dict");
    return v;
}
static const bencode::BencodeValue& expect_list(const bencode::BencodeValue& v, const char* where) {
    if (!v.isList()) throw std::runtime_error(std::string(where) + ": expected list");
    return v;
}
static const bencode::BencodeValue& expect_str(const bencode::BencodeValue& v, const char* where) {
    if (!v.isString()) throw std::runtime_error(std::string(where) + ": expected string");
    return v;
}
static const bencode::BencodeValue* find_key(const bencode::BencodeValue& dict, const char* key) {
    const auto& m = dict.asDict();
    auto it = m.find(key);
    return it == m.end() ? nullptr : &it->second;
}

static std::array<uint8_t,20> compute_infohash_from_slice(std::string_view raw) {
    return sha1_bytes(raw.data(), raw.size());
}

static std::vector<std::array<uint8_t,20>> split_pieces_blob(const std::string& blob) {
    if (blob.size() % 20 != 0) throw std::runtime_error("pieces blob not multiple of 20");

    std::vector<std::array<uint8_t,20>> out;
    out.reserve(blob.size() / 20);

    for (size_t i = 0; i < blob.size(); i += 20) {
        std::array<uint8_t,20> a{};
        std::memcpy(a.data(), blob.data() + i, 20);
        out.push_back(a);
    }
    return out;
}

static std::vector<FileEntry> single_file_entries(const bencode::BencodeValue& infoDict) {

    const auto* lenv = find_key(infoDict, "length");

    if (!lenv) throw std::runtime_error("info.length missing");
    if (!lenv->isInt()) throw std::runtime_error("info.length not int");
    
    uint64_t len = static_cast<uint64_t>(lenv->asInt());
    FileEntry fe;

    // path = name for single-file
    const auto* namev = find_key(infoDict, "name");
    if (!namev || !namev->isString()) throw std::runtime_error("info.name missing or not string");
   
    fe.path = std::filesystem::path(namev->asString());
    fe.length = len;
    fe.offset = 0;
    return {fe};
}

static std::vector<FileEntry> multi_file_entries(const bencode::BencodeValue& filesv) {

    const auto& lst = expect_list(filesv, "info.files").asList();
    std::vector<FileEntry> out;
    out.reserve(lst.size());
    uint64_t running = 0;

    for (const auto& fv : lst) {
        const auto& fd = expect_dict(fv, "file entry");
        
        const auto* lenv = find_key(fd, "length");
        if (!lenv || !lenv->isInt()) throw std::runtime_error("file.length missing or not int");
        uint64_t len = static_cast<uint64_t>(lenv->asInt());
        
        // path (list of strings)
        const auto* pathv = find_key(fd, "path");
        if (!pathv || !pathv->isList()) throw std::runtime_error("file.path missing or not list");
        std::filesystem::path p;
        for (const auto& segv : pathv->asList()) {
            const auto& s = expect_str(segv, "file.path segment").asString();
            p /= s;
        }
        
        FileEntry fe;
        fe.path = p;
        fe.length = len;
        fe.offset = running;
        running += len;
        out.push_back(std::move(fe));
    }

    return out;
}

static std::vector<std::vector<std::string>> collect_tracker_tiers(const bencode::BencodeValue& root) {
    std::vector<std::vector<std::string>> tiers;

    const auto* al = find_key(root, "announce-list");
    if (al && al->isList()) {
        // BEP 12: announce-list is a list of lists of strings
        for (const auto& tierVal : al->asList()) {
            if (!tierVal.isList()) continue;
            std::vector<std::string> tier;
            for (const auto& s : tierVal.asList()) {
                if (s.isString()) tier.push_back(s.asString());
            }
            if (!tier.empty()) tiers.push_back(std::move(tier));
        }
    }

    // If announce-list is absent or empty, fall back to single-tier "announce"
    if (tiers.empty()) {
        const auto* a = find_key(root, "announce");
        if (a && a->isString()) {
            tiers.push_back({ a->asString() });
        }
    }

    return tiers;
}

static std::string_view grab_info_slice(std::string_view data) {

    auto pr = bencode::BencodeParser::parseWithInfoSlice(data);
    if (!pr.infoSlice) throw std::runtime_error("missing 'info' dictionary");
    
    // Ensure root is a dict
    (void)expect_dict(pr.root, "root");
    return *pr.infoSlice; // raw bytes of "info" value
}

// Parse the already-decoded "info" dictionary into InfoDictionary
static InfoDictionary decode_info_dict(const bencode::BencodeValue& root, std::string_view infoSlice) {

    const auto& rdict = expect_dict(root, "root");
    const auto* info = find_key(rdict, "info");
   
    if (!info) throw std::runtime_error("root.info missing");
    const auto& infod = expect_dict(*info, "info");

    InfoDictionary out;
    out.rawSlice = infoSlice;

    if (auto* namev = find_key(infod, "name")) {
        out.name = expect_str(*namev, "info.name").asString();

    } else {
        throw std::runtime_error("info.name missing");
    }

    if (auto* plv = find_key(infod, "piece length")) {
        if (!plv->isInt()) throw std::runtime_error("info.piece length not int");
        auto val = plv->asInt();

        if (val <= 0) throw std::runtime_error("info.piece length <= 0");
        out.pieceLength = static_cast<uint32_t>(val);

    } else {
        throw std::runtime_error("info.piece length missing");
    }

    // pieces (20-byte concatenation)
    if (auto* pv = find_key(infod, "pieces")) {
        const auto& blob = expect_str(*pv, "info.pieces").asString();
        out.pieces = split_pieces_blob(blob);

    } else {
        throw std::runtime_error("info.pieces missing");
    }

    // files vs length
    if (auto* filesv = find_key(infod, "files")) {
        out.files = multi_file_entries(*filesv);

    } else {
        out.files = single_file_entries(infod);
    }

    return out;
}

// -------------------------- Public API ---------------------------

Metainfo Metainfo::fromTorrent(std::string_view data) {

    auto pr = bencode::BencodeParser::parseWithInfoSlice(data);
    const auto& root = expect_dict(pr.root, "root");

    Metainfo mi;

    if (!pr.infoSlice) throw std::runtime_error("missing 'info' dictionary");
    mi.info = decode_info_dict(root, *pr.infoSlice);

    mi.announceList = collect_tracker_tiers(root);

    // Compute infohash from exact raw bytes of "info"
    mi.infoHash_ = compute_infohash_from_slice(mi.info.rawSlice);

    return mi;
}

// Minimal magnet support: xt=urn:btih:<20-byte SHA1 (hex or base32)>, dn, tr
// This fills only infoHash_ (if present), announceList, and info.name (from dn).
// pieces/pieceLength/files remain empty until metadata fetch (outside scope here).

Metainfo Metainfo::fromMagnet(const std::string& uri) {
    Metainfo mi;

    auto pos = uri.find("magnet:?");
    if (pos != 0) throw std::runtime_error("invalid magnet URI");
    auto q = uri.substr(8); // skip "magnet:?"

    auto decode_pct = [](std::string_view s) {
        
        std::string out;
        out.reserve(s.size());

        for (size_t i = 0; i < s.size(); ++i) {
            
            if (s[i] == '%' && i + 2 < s.size()) {
                auto hex = s.substr(i+1, 2);
                int v = 0;
                auto hex_to_int = [](char c)->int{
                    if (c >= '0' && c <= '9') return c - '0';
                    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
                    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
                    return -1;
                };
                int hi = hex_to_int(hex[0]), lo = hex_to_int(hex[1]);
                if (hi >= 0 && lo >= 0) { out.push_back(char((hi<<4)|lo)); i += 2; continue; }
            }

            out.push_back(s[i]);
        }

        return out;
    };

    auto add_tracker = [&](std::string v) {
        // Check across all tiers
        for (auto& tier : mi.announceList) {
            if (std::find(tier.begin(), tier.end(), v) != tier.end())
                return; // already present
        }
        // Add as its own new tier
        mi.announceList.push_back({std::move(v)});
    };


    // parse query pairs
    size_t i = 8; // after "magnet:?"
    while (i < uri.size()) {

        size_t amp = uri.find('&', i);
        std::string_view kv = (amp == std::string::npos) ? std::string_view(uri).substr(i)
                                                         : std::string_view(uri).substr(i, amp - i);
        size_t eq = kv.find('=');
        std::string key = std::string(kv.substr(0, eq == std::string::npos ? kv.size() : eq));
        std::string val = (eq == std::string::npos) ? std::string() : decode_pct(kv.substr(eq + 1));

        if (key == "dn") {
            mi.info.name = std::move(val);

        } else if (key == "tr") {
            add_tracker(std::move(val));

        } else if (key == "xt") {

            // Expect xt=urn:btih:<hash>
            constexpr std::string_view prefix = "urn:btih:";
            if (val.rfind(prefix, 0) == 0) {
                auto h = std::string_view(val).substr(prefix.size());

                // Hex (40 chars)
                if (h.size() == 40) {
                    std::array<uint8_t,20> v{};
                    auto hex_to_byte = [](char c)->int{
                        if (c >= '0' && c <= '9') return c - '0';
                        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
                        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
                        return -1;
                    };

                    for (size_t j = 0; j < 20; ++j) {
                        int hi = hex_to_byte(h[2*j]);
                        int lo = hex_to_byte(h[2*j + 1]);
                        if (hi < 0 || lo < 0) throw std::runtime_error("invalid btih hex");
                        v[j] = static_cast<uint8_t>((hi<<4)|lo);
                    }
                    
                    mi.infoHash_ = v;
                } else {
                    // Base32 case (common) – implement or defer
                    // For now you can either implement base32 here or throw:
                    // throw std::runtime_error("btih base32 not yet supported");
                    // (Optionally: TODO base32 decode)
                }
            }
        }

        if (amp == std::string::npos) break;
        i = amp + 1;
    }

    return mi;
}