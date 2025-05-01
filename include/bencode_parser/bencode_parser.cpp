// Set of Bencoding Functions


#include "bencode_parser.hpp"

namespace Bencode
{
    // NOTE: Find an alternative version to populate bencode_tree_ with returned temp_val

    void Parser::parse(std::string_view byte_stream, std::string_view::const_iterator &it_curr, std::string_view::const_iterator &it_end)
    {
        BencodeAST temp_val = parse_stream(byte_stream, it_curr, it_end);
        bencode_tree_.type_ = temp_val.type_;
        std::swap(temp_val.value_, bencode_tree_.value_);

    }

    // const Parser::BencodeAST Parser::get_tree() const
    // {
    //     return bencode_tree_;
    // }

    const Parser::BencodeAST Parser::parse_stream(const std::string_view &byte_stream, std::string_view::const_iterator &it_curr, std::string_view::const_iterator &it_end)
    {
        
        if ((*it_curr == 'i') && (it_curr + 1) != it_end) return parse_int(byte_stream, it_curr, it_end);

        else if (std::isdigit(*it_curr) && (it_curr + 1) != it_end) return parse_string(byte_stream, it_curr, it_end);

        else if ((*it_curr == 'l') && it_curr++ != it_end) return parse_list(byte_stream, it_curr, it_end);

        // else if ((*it_curr == 'd') && it_curr++ != it_end) return parse_dict(byte_stream, it_curr, it_end);

        return BencodeAST();

    }

    const Parser::BencodeAST Parser::parse_int(const std::string_view &byte_stream, std::string_view::const_iterator &it_curr, std::string_view::const_iterator &it_end)
    {
        it_curr++;
        auto it_e = std::find(it_curr, it_end, 'e');

        if (it_e != it_end) {

            std::string number_str(it_curr, it_e);

            if (!is_strict_integer(number_str)) {
                throw std::runtime_error("Invalid Encoded Int String: i<" + number_str + ">e");
            }


            int64_t number = std::atoll(number_str.c_str());
            it_curr = it_e + 1;

            return BencodeAST(number);

        }
        else {

            std::string encoded_string(it_curr, it_end);
            throw std::runtime_error("Invalid Encoded Int String: i<" + encoded_string);
        }


    }
    
    const Parser::BencodeAST Parser::parse_string(const std::string_view &byte_stream, std::string_view::const_iterator &it_curr, std::string_view::const_iterator &it_end)
    {
        // retrieve length of string (encoded: Base 10 ASCII)
        
        auto it_colon = std::find(it_curr, it_end, ':');

        if (it_colon != it_end)
        {
            std::string str_length(it_curr, it_colon);
            

            if (!is_strict_positive_integer(str_length)) {
                throw std::runtime_error("Invalid encoded string length: <" + str_length);
            }

            char* end_ptr = nullptr;
            
            auto ul_length = std::strtoull(str_length.c_str(), &end_ptr, 10);
            size_t length_size_t = static_cast<size_t>(ul_length);

            it_colon++;
            std::string_view str = byte_stream.substr(static_cast<std::size_t>(it_colon - byte_stream.begin()), length_size_t);

            it_curr = it_colon + length_size_t + 1;

            return BencodeAST(str);
        }
        else
        {
            std::string encoded_string(it_curr, it_end);
            throw std::runtime_error("Invalid Encoded Int String: i<" + encoded_string);
        }
    }

    const Parser::BencodeAST Parser::parse_list(const std::string_view &byte_stream, std::string_view::const_iterator &it_curr, std::string_view::const_iterator &it_end)
    {
        // BUG: Uninitialized BencodeAST struct in temp_list
        std::vector<Parser::BencodeAST> temp_list;
        it_curr++;

        while (it_curr != it_end && *it_curr != 'e')
        {
            temp_list.push_back(Parser::parse_stream(byte_stream, it_curr, it_end));
            
        }

        if (it_curr != it_end)
        {
            return Parser::BencodeAST(temp_list);
        }
        else
        {
            throw std::runtime_error("Failed at parsing list at parse_list(...)");
        }

    }


}