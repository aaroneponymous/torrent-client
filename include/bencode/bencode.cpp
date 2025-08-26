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
        
        // int: i<digits>e ; no leading zeros (except zero), no -0
        expect('i');
        bool neg = false;
        if (peek() == '-') { get(); neg = true; }

        // at least one digit
        if (!(peek() >= '0' && peek() <= '9')) {
            throw parse_error("integer missing digits", pos_);
        }

        // leading zero rule
        size_t start_digits = pos_;

        if (peek() == '0') {
            get(); // single zero
            if (peek() != 'e') {
                // if there are more digits after '0', it's invalid
                throw parse_error("leading zeros not allowed", pos_);
            }
            // number is zero
            expect('e');
            if (neg) throw parse_error("negative zero not allowed", start_digits - 1);
            return BencodeValue(int64_t(0));
        }

        // parse digits
        int64_t value = 0;
        while (peek() >= '0' && peek() <= '9') {
            int d = get() - '0';
            // overflow check (conservative)
            if (value > (std::numeric_limits<int64_t>::max() - d) / 10) {
                throw parse_error("integer overflow", pos_);
            }
            value = value * 10 + d;
        }

        expect('e');
        if (neg) value = -value;
        return BencodeValue(value);
    }


    BencodeValue BencodeParser::parseString() {
        // string: <len>:<bytes>
        // len must be non-negative, no leading zeros unless zero itself is "0"
        // Here, since len is a positive length, "0" is valid and means empty string.
        // Read length
        size_t len = 0;
        // handle "0" specially; disallow "01"
        if (peek() == '0') {
            get(); // '0'
            expect(':');
            return BencodeValue(std::string{}); // empty
        }

        // first digit 1..9
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

        // extract bytes
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
        std::map<std::string,BencodeValue> dict;

        // Keys must be bencoded strings (byte strings)
        // (BEP‑3 allows any order; we store in std::map which sorts)

        while (peek() != 'e') {
            BencodeValue key = parseString();
            if (!key.isString()) throw parse_error("dict key not string", pos_);
            const std::string& k = key.asString();
            if (dict.find(k) != dict.end()) {
                // duplicate keys are not disallowed by BEP‑3 explicitly, but most clients reject.
                throw parse_error("duplicate dict key", pos_);
            }
            BencodeValue val = parseValue();
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
        // fastest int->string (std::to_string is fine here)
        out += std::to_string(x);
        out.push_back('e');
    }

    static void encode_string(const std::string& s, std::string& out) {
        // length:bytes
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
            encode_string(kv.first, out);   // key as bencoded string
            encode_impl(kv.second, out);    // value
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