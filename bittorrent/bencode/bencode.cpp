#include "bencode.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
#include <sstream>
#include <cassert>

namespace bencode {

    // ---------- BencodeValue ----------

    BencodeValue::BencodeValue() : type_(Type::None) {}

    BencodeValue::BencodeValue(int64_t i) : type_(Type::Int), intValue_(i) {}

    BencodeValue::BencodeValue(const std::string& s) : type_(Type::String), strValue_(s) {}

    BencodeValue::BencodeValue(std::string&& s) : type_(Type::String), strValue_(std::move(s)) {}

    BencodeValue::BencodeValue(const std::vector<BencodeValue>& l) : type_(Type::List), listValue_(l) {}

    BencodeValue::BencodeValue(std::vector<BencodeValue>&& l) : type_(Type::List), listValue_(std::move(l)) {}

    BencodeValue::BencodeValue(const std::map<std::string,BencodeValue>& d) : type_(Type::Dict), dictValue_(d) {}

    BencodeValue::BencodeValue(std::map<std::string,BencodeValue>&& d) : type_(Type::Dict), dictValue_(std::move(d)) {}

    bool BencodeValue::isInt()    const noexcept { return type_ == Type::Int; }
    bool BencodeValue::isString() const noexcept { return type_ == Type::String; }
    bool BencodeValue::isList()   const noexcept { return type_ == Type::List; }
    bool BencodeValue::isDict()   const noexcept { return type_ == Type::Dict; }

    int64_t BencodeValue::asInt() const {
    if (!isInt()) throw std::runtime_error("BencodeValue: not an int");
    return intValue_;
    }

    const std::string& BencodeValue::asString() const {
        if (!isString()) throw std::runtime_error("BencodeValue: not a string");
        return strValue_;
    }

    const std::vector<BencodeValue>& BencodeValue::asList() const {
        if (!isList()) throw std::runtime_error("BencodeValue: not a list");
        return listValue_;
    }

    const std::map<std::string,BencodeValue>& BencodeValue::asDict() const {
        if (!isDict()) throw std::runtime_error("BencodeValue: not a dict");
        return dictValue_;
    }


    std::string BencodeValue::toString() const {

        // Debug-friendly dump (not canonical bencode)

        switch (type_) {

            case Type::None:   return "null";
            case Type::Int:    return std::to_string(intValue_);

            case Type::String: {

                std::ostringstream oss;
                oss << '"';
                for (unsigned char c : strValue_) {
                    if (c == '\\' || c == '"') { oss << '\\' << char(c); }
                    else if (c < 0x20 || c >= 0x7F) {
                        oss << "\\x";
                        static const char* hex = "0123456789ABCDEF";
                        oss << hex[(c >> 4) & 0xF] << hex[c & 0xF];
                    } else {
                        oss << char(c);
                    }
                }
                oss << '"';
                return oss.str();

            }

            case Type::List: {

                std::string out = "[";
                bool first = true;
                for (auto& v : listValue_) {
                    if (!first) out += ", ";
                    first = false;
                    out += v.toString();
                }
                out += "]";
                return out;

            }

            case Type::Dict: {

                std::string out = "{";
                bool first = true;
                for (auto& kv : dictValue_) {
                    if (!first) out += ", ";
                    first = false;
                    // keys are raw bytes stored in std::string; print as JSON-ish string
                    BencodeValue keyStr(kv.first);
                    out += keyStr.toString();
                    out += ": ";
                    out += kv.second.toString();
                }
                out += "}";
                return out;

            }
        }

        return "null";
    }



    // ---------- BencodeParser ----------

    static std::runtime_error parse_error(const char* msg, size_t pos) {
        std::ostringstream oss;
        oss << "bencode parse error at " << pos << ": " << msg;
        return std::runtime_error(oss.str());
    }

    BencodeParser::BencodeParser(std::string_view input) : input_(input), pos_(0) {}

    ParseResult BencodeParser::parseWithInfoSlice(const std::string_view& input) {

        BencodeParser p(input);
        p.enableInfoSpanCapture(true);
        BencodeValue v = p.parseValue();

        if (p.pos_ != input.size()) {
            throw parse_error("trailing data after valid bencode", p.pos_);
        }

        ParseResult r{std::move(v), std::nullopt};
        if (auto s = p.infoSliceBytes()) {
            auto [ptr, len] = *s;
            r.infoSlice = std::string_view(ptr, len);
        }
        return r;
    }



    static void ensure_not_eof(std::string_view s, size_t pos) {
        if (pos >= s.size()) throw parse_error("unexpected end of input", pos);
    }

    char BencodeParser::peek() const {
        ensure_not_eof(input_, pos_);
        return input_[pos_];
    }

    char BencodeParser::get() {
        ensure_not_eof(input_, pos_);
        return input_[pos_++];
    }

    void BencodeParser::expect(char c) {
        char g = get();
        if (g != c) throw parse_error("unexpected character", pos_ - 1);
    }

    BencodeValue BencodeParser::parseValue() {
        char c = peek();
        if (c == 'i') return parseInt();
        if (c == 'l') return parseList();
        if (c == 'd') return parseDict();
        if (c >= '0' && c <= '9') return parseString();
        throw parse_error("invalid value prefix", pos_);
    }


    BencodeValue BencodeParser::parseInt() {
        expect('i');
        bool neg = false;
        if (peek() == '-') { get(); neg = true; }

        // At least one digit
        if (!(peek() >= '0' && peek() <= '9')) {
            throw parse_error("integer missing digits", pos_);
        }

        // Leading zero rules (allow "i0e", forbid "-0", forbid leading zeros)
        if (peek() == '0') {
            get(); // consume '0'
            expect('e');
            if (neg) throw parse_error("negative zero not allowed", pos_ - 2);
            return BencodeValue(int64_t(0));
        }

        // Parse magnitude into uint64_t
        uint64_t mag = 0;
        while (peek() >= '0' && peek() <= '9') {
            int d = get() - '0';
            // Conservative overflow for mag (uint64_t)
            if (mag > (std::numeric_limits<uint64_t>::max() - uint64_t(d)) / 10ULL) {
                throw parse_error("integer overflow", pos_);
            }
            mag = mag * 10ULL + uint64_t(d);
        }
        expect('e');

        if (!neg) {
            if (mag > uint64_t(std::numeric_limits<int64_t>::max()))
                throw parse_error("integer overflow", pos_);
            return BencodeValue(static_cast<int64_t>(mag));
        } else {

            // Allow INT64_MIN = -9223372036854775808
            constexpr uint64_t ABS_INT64_MIN = uint64_t(1) << 63; // 9223372036854775808

            if (mag == ABS_INT64_MIN) return BencodeValue(std::numeric_limits<int64_t>::min());
            if (mag > uint64_t(std::numeric_limits<int64_t>::max())) throw parse_error("integer overflow", pos_);
            return BencodeValue(-static_cast<int64_t>(mag));
        }
    }


    BencodeValue BencodeParser::parseString() {
        size_t len = 0;

        if (peek() == '0') {
            get();
            expect(':');
            return BencodeValue(std::string{});
        }

        if (!(peek() >= '1' && peek() <= '9')) {
            throw parse_error("invalid string length start", pos_);
        }
        while (peek() >= '0' && peek() <= '9') {
            int d = get() - '0';
            if (len > (std::numeric_limits<size_t>::max() - size_t(d)) / 10) {
                throw parse_error("string length overflow", pos_);
            }
            len = len * 10 + size_t(d);
            if (pos_ >= input_.size()) break;
            if (input_[pos_] == ':') break;
        }

        expect(':');

        if (input_.size() - pos_ < len) {
            throw parse_error("string length exceeds input", pos_);
        }
        std::string out;
        out.assign(input_.substr(pos_, len));
        pos_ += len;
        return BencodeValue(std::move(out));
    }

    BencodeValue BencodeParser::parseList() {
        expect('l');
        std::vector<BencodeValue> lst;
        while (peek() != 'e') {
            lst.push_back(parseValue());
        }
        expect('e');
        return BencodeValue(std::move(lst));
    }

    BencodeValue BencodeParser::parseDict() {
        expect('d');
        std::map<std::string, BencodeValue> dict;

        // Keys must be bencoded strings (byte strings).
        // (BEP-3 allows any order; we store in std::map which sorts)
        // to validate canonical order at parse-time, can compare consecutive keys and throw (see comment block below)

        std::optional<std::string> last_key; // only for optional canonical-order validation

        while (peek() != 'e') {
            BencodeValue key = parseString();
            const std::string& k = key.asString();

            // Optional canonical-order validation (disabled by default):
            // if (last_key && *last_key > k) {
            //     throw parse_error("dict keys out of order", pos_);
            // }
            // last_key = k;

            // Duplicate keys are generally rejected by clients(?) (more research)

            if (dict.find(k) != dict.end()) {
                throw parse_error("duplicate dict key", pos_);
            }

            // Parse value and capture exact byte span for "info" (when enabled).
            size_t val_begin = pos_;
            BencodeValue val = parseValue();
            size_t val_end = pos_;

            if (capture_info_span_ && k == "info" && !info_span_) {
                info_span_ = Span{val_begin, val_end};
            }

            dict.emplace(k, std::move(val));
        }

        expect('e');
        return BencodeValue(std::move(dict));
    }



    BencodeValue BencodeParser::parse(const std::string_view& input) {

        BencodeParser p(input);
        BencodeValue v = p.parseValue();
        
        /**
         *  @todo: ensure all input consumed (Optional)
        */

        if (p.pos_ != input.size()) {
            
            // allow trailing data if desired; here we’re strict:
            throw parse_error("trailing data after valid bencode", p.pos_);
        }
        return v;
    }

    // ---- Encoder ----

    static void encode_impl(const BencodeValue& v, std::string& out);

    static void encode_int(int64_t x, std::string& out) {
        out.push_back('i');
        out += std::to_string(x);
        out.push_back('e');
    }

    static void encode_string(const std::string& s, std::string& out) {
        out += std::to_string(s.size());
        out.push_back(':');
        out.append(s.data(), s.size());
    }

    static void encode_list(const std::vector<BencodeValue>& lst, std::string& out) {
        out.push_back('l');
        for (const auto& e : lst) encode_impl(e, out);
        out.push_back('e');
    }

    static void encode_dict(const std::map<std::string,BencodeValue>& dict, std::string& out) {
        out.push_back('d');
        // std::map keeps keys sorted lexicographically — canonical
        for (const auto& kv : dict) {
            encode_string(kv.first, out);
            encode_impl(kv.second, out);
        }
        out.push_back('e');
    }

    static void encode_impl(const BencodeValue& v, std::string& out) {

        switch (v.type()) {
            case BencodeValue::Type::None:
                throw std::runtime_error("cannot encode None");
            case BencodeValue::Type::Int:
                encode_int(v.asInt(), out);
                break;
            case BencodeValue::Type::String:
                encode_string(v.asString(), out);
                break;
            case BencodeValue::Type::List:
                encode_list(v.asList(), out);
                break;
            case BencodeValue::Type::Dict:
                encode_dict(v.asDict(), out);
                break;
        }
    }

    std::string BencodeParser::encode(const BencodeValue& val) {
        std::string out;
        out.reserve(256); // small headroom
        encode_impl(val, out);
        return out;
    }

}