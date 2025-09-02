#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <optional>



namespace bencode {

    class BencodeValue 
    {
    public:
        enum class Type { None, Int, String, List, Dict };

        BencodeValue();
        BencodeValue(int64_t i);
        BencodeValue(const std::string& s);
        BencodeValue(std::string&& s);
        BencodeValue(const std::vector<BencodeValue>& l);
        BencodeValue(std::vector<BencodeValue>&& l);
        BencodeValue(const std::map<std::string, BencodeValue>& d);
        BencodeValue(std::map<std::string, BencodeValue>&& d);


        bool isInt() const noexcept;
        bool isString() const noexcept;
        bool isList() const noexcept;
        bool isDict() const noexcept;


        int64_t asInt() const;
        const std::string& asString() const;
        const std::vector<BencodeValue>& asList() const;
        const std::map<std::string, BencodeValue>& asDict() const;

        std::string toString() const;
        Type type() const noexcept { return type_; }

    private:
        Type type_{Type::None};
        int64_t intValue_{0};
        std::string strValue_;
        std::vector<BencodeValue> listValue_;
        std::map<std::string, BencodeValue> dictValue_;
    };

    struct ParseResult 
    {
        BencodeValue root;
        std::optional<std::string_view> infoSlice;
    };


    class BencodeParser 
    {
    public:
        static BencodeValue parse(const std::string_view& input);
        static std::string encode(const BencodeValue& val);
        static ParseResult parseWithInfoSlice(const std::string_view& input);


    private:

        explicit BencodeParser(std::string_view input);

        // Recursive Descent
        BencodeValue parseValue();
        BencodeValue parseInt();
        BencodeValue parseString();
        BencodeValue parseList();
        BencodeValue parseDict();

        char peek() const;
        char get();
        void expect(char c);

        std::string_view input_;
        size_t pos_{0};

        struct Span { size_t begin{}, end{}; };
        bool capture_info_span_{false};
        std::optional<Span> info_span_;
        void enableInfoSpanCapture(bool on = true) { capture_info_span_ = on; }

        std::optional<std::pair<const char*, size_t>> infoSliceBytes() const {
            if (!info_span_) return std::nullopt;
            auto [b, e] = *info_span_;
            return std::make_pair(input_.data() + b, e - b);
        }

    };


}
