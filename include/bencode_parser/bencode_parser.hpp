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