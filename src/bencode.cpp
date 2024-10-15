

#include "../include/bittorrent/bencode.hpp"

namespace Bencode
{
    // Set of Decoding Functions

    auto decodeBencode(const std::string &encoded_string, size_t &pos) -> nlohmann::json
    {

        // std::cout << "decode_bencoded_value: " + encoded_string.substr(pos) << std::endl;

        if (std::isdigit(encoded_string[pos])) // Case: Char at [pos] is digit (string)
        {
            return Bencode::decodeStr(encoded_string, pos);
        }
        else if (encoded_string[pos] == 'i') // Case: Char at [pos] is 'i' (integer)
        {
            return Bencode::decodeInt(encoded_string, pos);
        }
        else if (encoded_string[pos] == 'l') // Case: Char at [pos] is 'l' (list)
        {
            return Bencode::decodeList(encoded_string, pos);
        }
        else if (encoded_string[pos] == 'd')
        {
            return Bencode::decodeDict(encoded_string, pos);
        }
        else
        {
            throw std::runtime_error("Unhandled encoded value: " + encoded_string);
        }
    }

    auto decodeInt(const std::string &encoded_string, size_t &pos) -> nlohmann::json
    {

        size_t e_index = encoded_string.find_first_of('e', pos);

        if (e_index != std::string::npos)
        {
            // Check for disallowed values
            std::string number_str = encoded_string.substr(pos + 1, e_index - 1);
            int64_t number = std::atoll(number_str.c_str());

            // Account for valid integer str conversions
            // Invalid: i-0e or trailing zeroes i002e

            // std::cout << "number_str: " + number_str + "\n";
            // std::cout << "number_str length: " << number_str.length() << "\n";

            if (number_str.length() > 1 && ((number_str[0] == '-' && number == 0) || (number_str[0] == '0' && number >= 0)))
            {
                throw std::runtime_error("Invalid encoded value: " + encoded_string);
            }

            pos = e_index + 1;
            return nlohmann::json(number);
        }
        else
        {
            throw std::runtime_error("Invalid encoded value: " + encoded_string);
        }
    }

    auto decodeStr(const std::string &encoded_string, size_t &pos) -> nlohmann::json
    {

        /**
         * @todo : Account for edge cases and exceptions
         *
         * Example :    "50i24e2:Hi"
         * Now Returns:  json: Hi - but an invalid bencoding
         *
         * Example :    "51:Hello"
         * Final Index:  2 - since in substr_next
         */

        size_t colon_index = encoded_string.find(':', pos);

        if (colon_index != std::string::npos)
        {
            int64_t str_length = std::atoll((encoded_string.substr(pos, colon_index - pos)).c_str());
            std::string str = encoded_string.substr(colon_index + 1, str_length);
            pos = colon_index + str_length + 1;

            return nlohmann::json(str);
        }
        else
        {
            throw std::runtime_error("Invalid encoded value: " + encoded_string);
        }
    }

    auto decodeList(const std::string &encoded_string, size_t &pos) -> nlohmann::json
    {

        pos++;

        nlohmann::json list = nlohmann::json::array();

        while (encoded_string[pos] != 'e')
        {
            list.push_back(decodeBencode(encoded_string, pos));
        }

        pos++;

        return list;
    }

    auto decodeDict(const std::string &encoded_string, size_t &pos) -> nlohmann::json
    {

        // encoded_value starts with pos at: "d ... "

        pos++; // Move position to one char after 'd'

        nlohmann::json dict = nlohmann::json::object();

        while (encoded_string[pos] != 'e')
        {
            // Check whether the key type is string
            if (!std::isdigit(encoded_string[pos]))
            {
                throw std::runtime_error("Key cannot be a non-string value" + encoded_string);
            }

            auto key = decodeBencode(encoded_string, pos);
            nlohmann::json val = (key == "pieces") ? Bencode::piecestoHashStr(encoded_string, pos) : Bencode::decodeBencode(encoded_string, pos);
            dict[key] = val;
        }

        pos++;

        return nlohmann::json(dict);
    }

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

    NOTE: The maximum number of bit of this integer is unspecified, but to handle it as a signed 64bit integer is mandatory to handle "large files" aka .torrent for more that 4Gbyte.
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

    // Encoding json objects in Bencode

    auto encodeBencode(const nlohmann::json &json_obj, std::string &encoded_output) -> void
    {
        // Dictionary to Int
        if (json_obj.is_object())
        {
            Bencode::encodeDict(json_obj, encoded_output);
        }
        else if (json_obj.is_array())
        {

            Bencode::encodeList(json_obj, encoded_output);
        }
        else if (json_obj.is_string())
        {

            Bencode::encodeStr(json_obj, encoded_output);
        }
        else if (json_obj.is_number())
        {

            Bencode::encodeInt(json_obj, encoded_output);
        }
        else
        {
            throw std::runtime_error("json_obj passed is not of the type: object_j, array_j, string_j or number_j");
        }

    }

    auto encodeStr(const nlohmann::json &json_obj, std::string &encoded_output) -> void 
    {
        // Assuming json_obj is of the right type i.e. string_t

        // nolhmann::json using UTF-8 Encoding and std::string::size() / length()
        // returns number of bytes rather than characters in the string
        // Store in a Local String ? Or Divide size of string_json / 8 Bytes 

        // json_obj.dump() -> Results in a string within a "double quotation"

        std::string local_string(json_obj.dump());
        local_string = local_string.substr(1, local_string.length() - 2);
        
        uint64_t str_len = local_string.length();
        encoded_output += std::to_string(str_len);
        encoded_output += local_string;
    }

    auto encodeInt(const nlohmann::json &json_obj, std::string &encoded_output) -> void 
    {
        std::string string_number = json_obj.dump();
        encoded_output += "i" + string_number + "e";
    }

    auto encodeList(const nlohmann::json &json_obj, std::string &encoded_output) -> void
    {
        encoded_output += "l";

        for (auto json_element : json_obj)
        {
            encodeBencode(json_element, encoded_output);
        }

        encoded_output += "e";
    }

    // nlohmann::json object_t (based on map or unordered_map
    // Enforces Lexographical Ordering of Keys (Avoid Resorting)
    
    auto encodeDict(const nlohmann::json &json_obj, std::string &encoded_output) -> void
    {
        encoded_output += "d";

        for (auto it = json_obj.begin(); it != json_obj.end(); it++)
        {
            Bencode::encodeBencode(it.key(), encoded_output);       // Encode Key       // Break Chain & call string right away? Since key is always string?
            Bencode::encodeBencode(it.value(), encoded_output);     // Encode Value
        }

        encoded_output += "e";
    }

    // Helper Functions Hash - Hexadecimal Conversions

    auto piecestoHashStr(const std::string &encoded_string, size_t &pos) -> nlohmann::json
    {

        /**
         * @todo : Account for edge cases and exceptions
         *
         * Example :    "50i24e2:Hi"
         * Now Returns:  json: Hi - but an invalid bencoding
         *
         * Example :    "51:Hello"
         * Final Index:  2 - since in substr_next
         */

        size_t colon_index = encoded_string.find(':', pos);

        if (colon_index != std::string::npos)
        {
            std::string number_string = encoded_string.substr(pos, colon_index - pos);
            uint64_t number = std::atoll(number_string.c_str());

            std::string piece_str = encoded_string.substr(colon_index + 1, number);

            if ((number % 20 != 0) && (piece_str.size() != number))
            {
                throw std::runtime_error("Invalid Length of string pieces: " + number);
            }

            // Given the number of pieces covert to string for each 20 Bytes Piece

            auto piece_count = number / 20;

            std::string pieces_output;

            // while (piece_count > 0)
            // {
            //     std::string piece_string = encoded_value.substr(0, 20);
            //     pieces_output += bytes_to_hex(piece_string);
            //     piece_count--;
            // }

            for (size_t i = 0; i < piece_count; ++i)
            {
                std::string piece_hash_binary = piece_str.substr(i * 20, 20);
                std::string piece_hash_hex = Bencode::bytesToHex(piece_hash_binary);
                pieces_output += piece_hash_hex;
                std::cout << "Piece " << i + 1 << " SHA-1: " << piece_hash_hex << std::endl;
            }

            // // Test on info_sample - Size of String equals (No of Pieces * 20) - will be 60 in pieces60:
            // if (number == 60)
            // {
            //     std::cout << "Size of String for Hash: " << str.length() << std::endl;
            // }

            pos = colon_index + number + 1;

            // if (str.length() == 20) // Likely a SHA-1 hash
            // {
            //     return json(bytes_to_hex(str)); // Convert to hex before returning
            // }

            return nlohmann::json(pieces_output);
        }
        else
        {
            throw std::runtime_error("Invalid encoded value: " + encoded_string);
        }
    }

    auto bytesToHex(const std::string &bytes_string) -> std::string
    {

        const char hex_digits[] = "0123456789abcdef";
        std::string output;

        output.reserve(bytes_string.length() * 2); // Each byte gets represented by 2 Hex

        for (unsigned char c : bytes_string)
        {
            output.push_back(hex_digits[c >> 4]);  // Get the first hex digit (higher nibble)
            output.push_back(hex_digits[c & 0xf]); // Get the second hex digit (lower nibble)
        }

        return output;
    }

    // Torrent Parsers
    auto parseTorrent(const std::string &path, size_t &pos) -> nlohmann::json
    {

        std::streampos size;
        char *buffer;

        std::ifstream file(path, std::ios::in | std::ios::binary | std::ios::ate);
        nlohmann::json torrent_info;

        if (file.is_open())
        {

            size = file.tellg();
            int buff_size = file.tellg();
            buffer = new char[size];
            file.seekg(0, std::ios::beg);
            file.read(buffer, size);

            nlohmann::json torrent_info;
            std::string torrent_info_str(buffer, buff_size);

            try
            {
                size_t position = 0;
                torrent_info = Bencode::decodeBencode(torrent_info_str, position);
            }
            catch (...)
            {

                std::cout << "Exception in parse_torrent(): " << std::endl;
            }

            delete[] buffer;
            file.close();

            return torrent_info;
        }
        else
        {
            throw std::runtime_error("Unable to open file: " + path);
        }
    }

    auto infoTorrent(const std::string &path, size_t &pos) -> std::vector<std::string>
    {
        nlohmann::json torrent_table = parseTorrent(path, pos);
        nlohmann::json test_table = torrent_table;
        auto torrent_info = torrent_table.find("info");

        std::cout << "\nEncoding Functions Test:\n";
        std::string encoding_string;
        Bencode::encodeBencode(torrent_table, encoding_string);
        std::cout << "Bencoded String: " << encoding_string << std::endl;

        // Retrieve Values for Keys:
        auto tracker_it = torrent_table.find("announce"); // Tracker Link
        auto length_it = torrent_info->find("length");    // Length
        auto pieces_it = torrent_info->find("pieces");    // Pieces

        if (tracker_it != torrent_table.end() && length_it != torrent_info->end() && pieces_it != torrent_info->end())
        {
            nlohmann::json link_j = tracker_it.value();
            nlohmann::json length_j = length_it.value();
            nlohmann::json pieces_j = pieces_it.value();    

            std::string link = link_j.dump();
            std::string length = length_j.dump();
            std::string pieces = pieces_j.dump();

            std::vector<std::string> vector_info;
            vector_info.push_back(link.substr(link.find_first_of("\"") + 1, link.find_last_of("\"") - 1));
            vector_info.push_back(length);
            vector_info.push_back(pieces.substr(pieces.find_first_of("\"") + 1, pieces.find_last_of("\"") - 1));

            return vector_info;

            // std::string link = link_j.dump();
            // std::string length = length_j.dump();

            // std::vector<std::string> output = {link.substr(1, link.length() - 2), length};
            // return output;
        }

        return {};
    }

}