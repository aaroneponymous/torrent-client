// Set of Bencoding Functions

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

        // else if (std::isdigit(*it_curr) && (it_curr + 1) != it_end) return parse_string(byte_stream, it_curr, it_end);

        // else if ((*it_curr == 'l') && it_curr++ != it_end) return parse_list(byte_stream, it_curr, it_end);

        // else if ((*it_curr == 'd') && it_curr++ != it_end) return parse_dict(byte_stream, it_curr, it_end);

        return BencodeAST();

    }


    

    const Parser::BencodeAST Parser::parse_int(const std::string_view &byte_stream, std::string_view::const_iterator &it_curr, std::string_view::const_iterator &it_end)
    {

        auto it_e = std::find(it_curr++, it_end, 'e');

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
    
    // const Parser::BencodeAST Parser::parse_string(const std::string_view &byte_stream, std::string_view::const_iterator &it_curr, std::string_view::const_iterator &it_end)
    // {
        
    // }
}