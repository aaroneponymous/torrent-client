#pragma once
#include <iostream>
#include <vector>
#include <map>
#include <variant>
#include <string_view>
#include <regex>
#include <algorithm>
#include <stdexcept>


namespace Bencode
{
    class Parser
    {
        public:

            struct BencodeItem {
                enum class Type { Integer, String, List, Dict, Empty };

                using BencodeNode = std::variant<int64_t, std::string_view, std::vector<BencodeItem>, 
                std::map<std::string_view, BencodeItem>>;
                
                Type type_;
                BencodeNode value_;

                BencodeItem() : type_(Type::Empty) {}

                template <typename T>
                BencodeItem(T&& val) {
                    using Decayed = std::decay_t<T>;

                    if constexpr (std::is_same_v<Decayed, int64_t>) {
                        type_ = Type::Integer;
                    } else if constexpr (std::is_same_v<Decayed, std::string_view>) {
                        type_ = Type::String;
                    } else if constexpr (std::is_same_v<Decayed, std::vector<BencodeItem>>) {
                        type_ = Type::List;
                    } else if constexpr (std::is_same_v<Decayed, std::map<std::string_view, BencodeItem>>) {
                        type_ = Type::Dict;
                    } else {
                        static_assert(always_false<T>::value, "Unsupported type for BencodeItem");
                    }

                    value_ = std::forward<T>(val);
                }

                private:

                    // Utility for static_assert fallback

                    template <typename>
                    struct always_false : std::false_type {};

            };

            Parser() = default;
            ~Parser() = default;


            void parse(std::string_view byte_stream, std::string_view::const_iterator &it_curr, std::string_view::const_iterator &it_end);

            // using BencodeNode = std::variant<int64_t, std::string_view, std::vector<BencodeItem>, 
            //     std::map<std::string_view, BencodeItem>>;

            const BencodeItem get_tree() const;
        
        // private:
            
            const BencodeItem parse_stream(const std::string_view &byte_stream, std::string_view::const_iterator &it_curr, std::string_view::const_iterator &it_end);
            const BencodeItem parse_int(const std::string_view &byte_stream, std::string_view::const_iterator &it_curr, std::string_view::const_iterator &it_end);
            const BencodeItem parse_string(const std::string_view &byte_stream, std::string_view::const_iterator &it_curr, std::string_view::const_iterator &it_end);
            const BencodeItem parse_list(const std::string_view &byte_stream, std::string_view::const_iterator &it_curr, std::string_view::const_iterator &it_end);
            const BencodeItem parse_dict(const std::string_view &byte_stream, std::string_view::const_iterator &it_curr, std::string_view::const_iterator &it_end);

            inline bool is_strict_integer(const std::string& str) {
                static const std::regex strict_int_regex(
                    R"(^(0|-?[1-9]\d*)$)"
                );
                return std::regex_match(str, strict_int_regex);
            }

        public:

            BencodeItem bencode_tree_;
    };

    // using BencodeNode = std::variant<int64_t, std::string_view, std::vector<Parser::BencodeItem>, 
    //             std::map<std::string_view, Parser::BencodeItem>>;

}