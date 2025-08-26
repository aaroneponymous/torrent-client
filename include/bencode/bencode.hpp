#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <optional>

/**
 * @todo: Consider std::variant for BencodeValue member
 *        - Better Memory Usage
 * 
 */

namespace bencode {

    class BencodeValue {
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


    class BencodeParser {
    public:
        static BencodeValue parse(const std::string_view& input);
        static std::string encode(const BencodeValue& val);

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

    };


}
