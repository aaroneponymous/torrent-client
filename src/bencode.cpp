#include "../include/bittorrent/bencode.hpp"

namespace Bencode
{
    // Set of Decoding Functions

    nlohmann::json decodeEncoding(std::string::const_iterator &it_begin, std::string::const_iterator &it_end)
    {
        std::cout << "decodeEncoding Entered\n";
        if (std::isdigit(*it_begin))
        {
            return Bencode::decodeString(it_begin, it_end);
            
        }
        else if (*it_begin == 'i')
        {
            return Bencode::decodeInteger(it_begin, it_end);
        }
        else if (*it_begin == 'l')
        {
            return Bencode::decodeListing(it_begin, it_end);
        }
        else if (*it_begin == 'd')
        {
            return Bencode::decodeDictionary(it_begin, it_end);
        }
        else
        {
            std::string encoded_string(it_begin, it_end);
            throw std::runtime_error("Unhandled encoded value: " + encoded_string);
        }
    }

    nlohmann::json decodeBencode(const std::string &encoded_string, size_t &pos)
    {

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

    
    nlohmann::json decodeInteger(std::string::const_iterator &it_begin, std::string::const_iterator &it_end)
    {
        // [ ]: Exception Handling Needs to Be Done


        auto it_e = std::find(it_begin, it_end, 'e');

        if (it_e != it_end)
        {
            std::string number_str(++it_begin, it_e);
            int64_t number = std::atoll(number_str.c_str());
            it_begin = it_e + 1;

            return nlohmann::json(number);
        }
        else
        {
            std::string encoded_string(it_begin, it_end);
            throw std::runtime_error("Invalid Encoded Int Value: " + encoded_string); 
        }
    }

    nlohmann::json decodeInt(const std::string &encoded_string, size_t &pos)
    {
        size_t e_index = encoded_string.find_first_of('e', pos);

        if (e_index != std::string::npos)
        {
            // Check for disallowed values (Maybe Required Later On)
            // Account for valid integer str conversions
            // Invalid: i-0e or trailing zeroes i002e

            std::string number_str = encoded_string.substr(pos + 1, e_index - 1);
            int64_t number = std::atoll(number_str.c_str());

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

    nlohmann::json decodeString(std::string::const_iterator &it_begin, std::string::const_iterator &it_end)
    {
        auto it_colon = std::find(it_begin, it_end, ':');

        if (it_colon != it_end)
        {
            std::string substr(it_begin, it_colon);
            int64_t str_length = std::atoll(substr.c_str());
            it_begin = it_colon + 1;
            it_colon = it_begin + str_length;
            
            std::string value(it_begin, it_colon);
            it_begin = it_colon;

            return nlohmann::json(value);
        }
        else
        {
            std::string encoded_string(it_begin, it_end);
            throw std::runtime_error("Invalid Encoded String Value: " + encoded_string); 
        }
    }

    nlohmann::json decodeStr(const std::string &encoded_string, size_t &pos)
    {

        /**
         * // TODO: Account for edge cases and exceptions
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

            // [ ] Check:
            // 1. str of the right length - str_length (e.g. 5:helloooooo)

            pos = colon_index + str_length + 1;

            return nlohmann::json(str);
        }
        else
        {
            throw std::runtime_error("Invalid encoded value: " + encoded_string);
        }
    }

    nlohmann::json decodeListing(std::string::const_iterator &it_begin, std::string::const_iterator &it_end)
    {
        nlohmann::json list = nlohmann::json::array();
        
        while (++it_begin != it_end && *it_begin != 'e')
        {
            list.push_back(decodeEncoding(it_begin, it_end));

            if (it_begin == it_end)
            {
                std::string encoded_string(it_begin, it_end);
                throw std::runtime_error("Invalid encoded value [No Ending Delimiter 'e' found]: " + encoded_string);
            }

            // [ ]: Find a better more readable way to ensure correctness of iterator position

            it_begin--;
        }

        ++it_begin;
        
        return list;
    }

    nlohmann::json decodeList(const std::string &encoded_string, size_t &pos)
    {

        pos++;

        nlohmann::json list = nlohmann::json::array();

        // Invalid str: d<...> [no ending delimiter 'e']
        // Results in index out of bounds - within the other functions?

        while (encoded_string[pos] != 'e')
        {
            list.push_back(decodeBencode(encoded_string, pos));

            if (pos == std::string::npos)
            {
                throw std::runtime_error("Invalid encoded value [No Ending Delimiter 'e' found]: " + encoded_string);
            }
        }

        pos++;

        return list;
    }

    nlohmann::json decodeDictionary(std::string::const_iterator &it_begin, std::string::const_iterator &it_end)
    {
        nlohmann::json dict = nlohmann::json::object();

        while (++it_begin != it_end && *it_begin != 'e')
        {
            if (!std::isdigit(*it_begin))
            {
                std::string encoded_string(it_begin, it_end);
                throw std::runtime_error("Key cannot be a non-string value" + encoded_string);
            }

            auto key = decodeEncoding(it_begin, it_end);
            nlohmann::json val = (key == "pieces") ? piecesToHashStr(it_begin, it_end) : decodeEncoding(it_begin, it_end);

            std::cout << "\ndecodeDictionary val dump: " << val.dump() << "\n";

            dict[key] = val;

            // [ ]: Improve on decrementing it_begin to point back to correct the incremented it_begin

            it_begin--;
        }

        ++it_begin;
        
        return nlohmann::json(dict);
    }

    // [ ]: Unfinished Needs Proper Implementation for All Keys?

    nlohmann::json decodeDict(const std::string &encoded_string, size_t &pos)
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

            // [ ]: Value for pieces - return type list of strings (improvement in infoTorrent())

            nlohmann::json val = (key == "pieces") ? Bencode::piecestoHashStr(encoded_string, pos) : Bencode::decodeBencode(encoded_string, pos);

            // [ ]: Use map iterator method to insert (prevent unexpected addition of default value)

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

    // Encoding json objects in Bencode

    auto encodeBencode(const nlohmann::json &json_obj, std::string &encoded_output) -> void
    {

        if (json_obj.is_object())
        {
            encodeDict(json_obj, encoded_output);
        }
        else if (json_obj.is_array())
        {
            encodeList(json_obj, encoded_output);
        }
        else if (json_obj.is_string())
        {
            encodeStr(json_obj, encoded_output);
        }
        else if (json_obj.is_number())
        {
            encodeInt(json_obj, encoded_output);
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
        encoded_output += ":";
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
            if (it.key() != "pieces")
            {
                encodeBencode(it.key(), encoded_output);   // Encode Key       // RECHECK: (Optimization) Break Chain & call string right away? Since key is always string?
                encodeBencode(it.value(), encoded_output); // Encode Value
            }
            else
            {
                encodeBencode(it.key(), encoded_output); // Encode Key       // RECHECK: (Optimization) Break Chain & call string right away? Since key is always string?
                std::string byte_string = hexToBytes(it.value());
                encoded_output += std::to_string(byte_string.length());
                encoded_output += ":";
                encoded_output += hexToBytes(it.value());
            }
        }

        encoded_output += "e";
    }

    // Helper Functions Hash - Hexadecimal Conversions

    nlohmann::json piecesToHashStr(std::string::const_iterator &it_begin, std::string::const_iterator &it_end)
    {
        auto it_colon = std::find(it_begin, it_end, ':');

        if (it_colon != it_end)
        {
            std::string number_string(it_begin, it_colon);
            int64_t string_length = std::atoll(number_string.c_str());
            std::string pieces_string(it_colon + 1, it_colon + string_length + 1);
            std::cout << "piecesToHashStr pieces_string: " << pieces_string;

            if ((string_length % 20 != 0) && (pieces_string.size() != string_length))   // [ ] Redundant check? pieces_string.size() != string_length
            {
                throw std::runtime_error("Invalid Length of pieces string: " + string_length);
            }

            auto pieces_count = string_length / 20;
            std::string pieces_output;
            pieces_output.reserve(string_length * 2);

            std::string pieces_hex = bytesToHex(pieces_output);

            it_begin = it_colon + 1;

            return nlohmann::json(pieces_output);
        }
        else
        {
            std::string encoded_string(it_begin, it_end);
            throw std::runtime_error("Invalid encoded value: " + encoded_string);
        }

    }

    nlohmann::json piecestoHashStr(const std::string &encoded_string, size_t &pos)
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

            // HACK: BytesToHex the entire piece str                                        (1)

            // pieces_output = Bencode::bytesToHex(piece_str);

            std::string pieces_output;
            pieces_output.reserve(number * 2);

            for (size_t i = 0; i < piece_count; ++i)
            {
                std::string piece_hash_binary = piece_str.substr(i * 20, 20);
                std::string piece_hash_hex = Bencode::bytesToHex(piece_hash_binary);
                pieces_output += piece_hash_hex;
            }

            pos = colon_index + number + 1;

            return nlohmann::json(pieces_output);
        }
        else
        {
            throw std::runtime_error("Invalid encoded value: " + encoded_string);
        }
    }

    auto bytesToHex(const std::string &bytes_string) -> std::string
    {

        const unsigned char hex_digits[] = "0123456789abcdef";
        std::string output;

        output.reserve(bytes_string.length() * 2); // Each byte gets represented by 2 Hex

        for (unsigned char c : bytes_string)
        {
            output.push_back(hex_digits[c >> 4]);  // Get the first hex digit (higher nibble)
            output.push_back(hex_digits[c & 0xf]); // Get the second hex digit (lower nibble)
        }

        return output;
    }

    // LEARN: Simpler Implementation of Hexadecimal to Byte String

    auto hexToBytes(const std::string &hex_string) -> std::string
    {
        // Check Even Length
        if (hex_string.length() % 2 != 0)
        {
            throw std::invalid_argument("Hex String must have an even length");
        }

        std::string output;
        output.reserve(hex_string.length() / 2);

        for (size_t i = 0; i < hex_string.length(); i += 2)
        {
            char high_nibble = hex_string[i];
            char low_nibble = hex_string[i + 1];

            // Check chars valid hexadecimal digits
            if (!std::isxdigit(high_nibble) || !std::isxdigit(low_nibble))
            {
                // std::cout << "High Nibble Char: " << high_nibble << " Low Nibble Char: " << low_nibble << std::endl;
                throw std::invalid_argument("Hex String contains invalid characters.");
            }

            unsigned char byte = (std::stoi(std::string(1, high_nibble), nullptr, 16) << 4) |
                                 std::stoi(std::string(1, low_nibble), nullptr, 16);

            output.push_back(static_cast<char>(byte));
        }

        return output;
    }

    // Torrent Parser

    nlohmann::json parseTorrent(const std::string &path)
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

            std::string torrent_info_str(buffer, buff_size);
            auto it_begin = torrent_info_str.cbegin();
            auto it_end = torrent_info_str.cend();

            try
            {
                torrent_info = decodeEncoding(it_begin, it_end);
              
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

    auto infoTorrent(const std::string &path, size_t &pos) -> const std::vector<std::string>
    {
        nlohmann::json torrent_table = parseTorrent(path);
        auto torrent_info = torrent_table.find("info"); // Return Type: Iterator

        std::string info_bencoded_str;
        Bencode::encodeBencode(*torrent_table.find("info"), info_bencoded_str);

        // sha1.hpp used here to calculate info hash

        SHA1 checksum;
        checksum.update(info_bencoded_str);
        std::string info_hash = checksum.final();

        // Retrieve Values for Keys:
        // NOTE: torrent_table on the stack - Dot Operator to access objects/elements
        // NOTE: torrent_info iterator type pointing to objects on the heap - Arrow Operator to access objects/elements

        auto tracker_it = torrent_table.find("announce");       // iterator to Tracker Link
        auto length_it = torrent_info->find("length");          // iterator to Length
        auto pieceLen_it = torrent_info->find("piece length");
        auto pieces_it = torrent_info->find("pieces");          // iterator to Pieces

        if (tracker_it != torrent_table.end() && length_it != torrent_info->end() && pieces_it != torrent_info->end() && pieceLen_it != torrent_info->end())
        {
            std::string link = (tracker_it.value()).dump();
            std::string length = (length_it.value()).dump();
            std::string piece_length = (pieceLen_it.value().dump());
            std::string pieces = (pieces_it.value()).dump();

            pieces = pieces.substr(pieces.find_first_of("\"") + 1, pieces.find_last_of("\"") -1);


            std::vector<std::string> vector_info;
            vector_info.push_back(link.substr(link.find_first_of("\"") + 1, link.find_last_of("\"") - 1));
            vector_info.push_back(length);
            vector_info.push_back(info_hash);
            vector_info.push_back(piece_length);

            // HACK: Hard-Coded no of pieces = 3, make it dynamic later

            for (size_t i = 0; i < pieces.size(); i+=40)
            {
                vector_info.push_back(pieces.substr(i, 40));
            }

            return vector_info;
        }

        return {};
    }

}