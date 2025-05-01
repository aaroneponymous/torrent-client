#pragma once
#include <iostream>
#include <vector>
#include <map>
#include <variant>
#include <string_view>
#include <regex>
#include <algorithm>
#include <stdexcept>

/*

Byte Strings

Byte strings are encoded as follows: <string length encoded in base ten ASCII>:<string data>
Note that there is no constant beginning delimiter, and no ending delimiter.

Example: 4: spam represents the string "spam"
Example: 0: represents the empty string ""

Integers
Integers are encoded as follows: i<integer encoded in base ten ASCII>e
The initial i and trailing e are beginning and ending delimiters.

Example: i3e represents the integer "3"
Example: i-3e represents the integer "-3"
i-0e is invalid. All encodings with a leading zero, such as i03e, are invalid, other than i0e, which of course corresponds to the integer "0".

ATTENTION: The maximum number of bit of this integer is unspecified, but to handle it as a signed 64bit integer is mandatory to handle "large files" aka .torrent for more that 4Gbyte.
Lists
Lists are encoded as follows: l<bencoded values>e
The initial l and trailing e are beginning and ending delimiters. Lists may contain any bencoded type, including integers, strings, dictionaries, and even lists within other lists.

Example: l4:spam4:eggse represents the list of two strings: [ "spam", "eggs" ]
Example: le represents an empty list: []
Dictionaries
Dictionaries are encoded as follows: d<bencoded string><bencoded element>e
The initial d and trailing e are the beginning and ending delimiters. Note that the keys must be bencoded strings. The values may be any bencoded type, including integers, strings, lists, and other dictionaries. Keys must be strings and appear in sorted order (sorted as raw strings, not alphanumerics). The strings should be compared using a binary comparison, not a culture-specific "natural" comparison.

Example: d3:cow3:moo4:spam4:eggse represents the dictionary { "cow" => "moo", "spam" => "eggs" }
Example: d4:spaml1:a1:bee represents the dictionary { "spam" => [ "a", "b" ] }
Example: d9:publisher3:bob17:publisher-webpage15:www.example.com18:publisher.location4:homee represents { "publisher" => "bob", "publisher-webpage" => "www.example.com", "publisher.location" => "home" }
Example: de represents an empty dictionary {}

*/


namespace Bencode
{
    class Parser
    {
        public:

            struct BencodeAST {
                enum class Type { Integer, String, List, Dict, Empty };

                using Node = std::variant<int64_t, std::string_view, std::vector<BencodeAST>, 
                std::map<std::string_view, BencodeAST>>;
                
                Type type_;
                Node value_;

                BencodeAST() : type_(Type::Empty) {}

                template <typename T>
                BencodeAST(T&& val) {
                    using Decayed = std::decay_t<T>;

                    if constexpr (std::is_same_v<Decayed, int64_t>) {
                        type_ = Type::Integer;
                    } else if constexpr (std::is_same_v<Decayed, std::string_view>) {
                        type_ = Type::String;
                    } else if constexpr (std::is_same_v<Decayed, std::vector<BencodeAST>>) {
                        type_ = Type::List;
                    } else if constexpr (std::is_same_v<Decayed, std::map<std::string_view, BencodeAST>>) {
                        type_ = Type::Dict;
                    } else {
                        static_assert(always_false<T>::value, "Unsupported type for BencodeAST");
                    }

                    value_ = std::forward<T>(val);
                }

                // // Copy constructor
                // BencodeAST(const BencodeAST& other)
                // : type_(other.type_), value_(other.value_) {}

                // // Copy assignment
                // BencodeAST& operator=(const BencodeAST& other) {
                //     if (this != &other) {
                //         type_ = other.type_;
                //         value_ = other.value_;
                //     }
                //     return *this;
                // }

                // // Move constructor
                // BencodeAST(BencodeAST&& other) noexcept
                //     : type_(other.type_), value_(std::move(other.value_)) {}

                // // Move assignment
                // BencodeAST& operator=(BencodeAST&& other) noexcept {
                //     if (this != &other) {
                //         type_ = other.type_;
                //         value_ = std::move(other.value_);
                //     }
                //     return *this;
                // }
                

                private:

                    // Utility for static_assert fallback

                    template <typename>
                    struct always_false : std::false_type {};

            };

            Parser()
            {
                bencode_tree_ = BencodeAST(static_cast<int64_t>(0));
            }

            ~Parser() = default;

            void parse(std::string_view byte_stream, std::string_view::const_iterator &it_curr, std::string_view::const_iterator &it_end);

            // using BencodeAST = std::variant<int64_t, std::string_view, std::vector<BencodeAST>, 
            //     std::map<std::string_view, BencodeAST>>;

            const BencodeAST get_tree() const;
        
        // private:
            
            const BencodeAST parse_stream(const std::string_view &byte_stream, std::string_view::const_iterator &it_curr, std::string_view::const_iterator &it_end);
            const BencodeAST parse_int(const std::string_view &byte_stream, std::string_view::const_iterator &it_curr, std::string_view::const_iterator &it_end);
            const BencodeAST parse_string(const std::string_view &byte_stream, std::string_view::const_iterator &it_curr, std::string_view::const_iterator &it_end);
            const BencodeAST parse_list(const std::string_view &byte_stream, std::string_view::const_iterator &it_curr, std::string_view::const_iterator &it_end);
            const BencodeAST parse_dict(const std::string_view &byte_stream, std::string_view::const_iterator &it_curr, std::string_view::const_iterator &it_end);

            inline bool is_strict_integer(const std::string& str) {
                static const std::regex strict_int_regex(
                    R"(^(0|-?[1-9]\d*)$)"
                );
                return std::regex_match(str, strict_int_regex);
            }

            inline bool is_strict_positive_integer(const std::string& str) {
                static const std::regex strict_positive_int_regex(
                    R"(^(0|[1-9]\d*)$)"
                );
                return std::regex_match(str, strict_positive_int_regex);
            }
            

        public:

            BencodeAST bencode_tree_;
    };

    // using BencodeAST = std::variant<int64_t, std::string_view, std::vector<Parser::BencodeAST>, 
    //             std::map<std::string_view, Parser::BencodeAST>>;

}