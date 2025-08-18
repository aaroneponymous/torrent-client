#include "../include/bittorrent/bencode.hpp"

namespace Bencode
{

    nlohmann::json decodeBencode(const std::string_view &encoded_string, size_t &pos)
    {
        if (encoded_string.size() > 0) {

            if (std::isdigit(encoded_string[pos])) // Case: Char at [pos] is digit (string)
            {
                return Bencode::decodeString(encoded_string, pos);
            }
            else if (encoded_string[pos] == 'i') // Case: Char at [pos] is 'i' (integer) 
            {
                return Bencode::decodeInteger(encoded_string, pos);
            }
            else if (encoded_string[pos] == 'l')
            {
                return Bencode::decodeList(encoded_string, pos);
            }
            else if (encoded_string[pos] == 'd')
            {
                return Bencode::decodeDict(encoded_string, pos);
            }
            else 
            {
                throw std::runtime_error("Unhandled encoded value: ");
            }
        }
        else {
            throw std::runtime_error("Invalid encoded value: Empty String");
        }

    }

    bool is_all_digits(const std::string &s) {
        return std::all_of(s.begin(), s.end(), [](unsigned char c){ return std::isdigit(c); });
    }

    /**
     * @todo: Edge Cases Handling:
     * 1. i0eextra
     * 2. Overflow
     */

    nlohmann::json decodeInteger(const std::string_view &encoded_string, size_t &pos) {
        
        pos++;       
        size_t e_index = encoded_string.find_first_of('e', pos);

        if (e_index != std::string::npos) {

            /**
             * @todo: Edge Cases
             * 1. i12a3e - Malformed : Check if Valid Numeric String (done)
             * 2. Overflow (needs to be implemented)
             * 
             * 
            */

            std::string no_str(encoded_string.substr(pos, e_index - pos));
            bool is_valid = true;

            if (no_str[0] == '-') {
                std::string neg_str(no_str.substr(1, no_str.size() - 2));
                is_valid = is_all_digits(neg_str);

            } else {
                is_valid = is_all_digits(no_str);
            }

            if (!is_valid) throw std::runtime_error("Is not all digit");

            int64_t no_val = std::stoll(no_str);

            if (no_str.length() > 1 && ((no_str[0] == '-' && no_str[1] == '0') || (no_str[0] == '0' && no_val >= 0)))
            {
                throw std::runtime_error("Invalid encoded value: " + no_str);
            }

            pos = e_index + 1;
            return nlohmann::json(no_val);            
        }
        else {
            throw std::runtime_error("Invalid Encoding: No delimiter 'e' found: " + std::string(encoded_string.substr(pos, encoded_string.size())));
        }
    }


    /**
     * @todo: Ensure non-negative check is made in upper level functions
     * @todo: Edge Cases Handling
     * 4:sp\am          (backslashes not valid escapes in Bencode)
     * 3:💾             (multibyte emoji: visually one char, but 4 bytes (not 3:))
     */

    nlohmann::json decodeString(const std::string_view &encoded_string, size_t &pos)
    {
        size_t colon_index = encoded_string.find_first_of(':', pos);

        /**
         * std::string npos
         * npos is a static member constant value with the greatest possible value for an element of type size_t.
         * As a return value, it is usually used to indicate no matches.
         * This constant is defined with a value of -1, which because size_t is an unsigned integral type, it is the largest possible representable value for this type.
         */

        if (colon_index != std::string_view::npos)
        {
            std::string len_str(encoded_string.substr(pos, colon_index - pos));
            uint64_t len_int = std::atoll(len_str.c_str());

            size_t size_str = static_cast<size_t>(len_int);

            if ((len_int == 0 && len_str.size() > 1) || (len_int > 0 && len_str[0] == '0')) {
                throw std::runtime_error("decodeString: length of string trailing type: " + len_str);
            }

            size_t pos_end = colon_index + size_str;

            std::string decoded_res(encoded_string.substr(colon_index + 1, size_str));
            size_t decoded_len = decoded_res.size();

            if (decoded_len != size_str) {
                throw std::runtime_error("decodeString: Invalid Input - string size doesn't match the following value");
            }

            pos = pos_end + 1;

            return nlohmann::json(decoded_res);
        }
        else
        {
            std::string invalid_encoding(encoded_string);
            throw std::runtime_error("Invalid encoded value: " + invalid_encoding);
        }
    }


    nlohmann::json decodeList(const std::string_view &encoded_string, size_t &pos)
    {
        pos++;

        nlohmann::json list = nlohmann::json::array();
        if (pos == std::string::npos) {
            throw std::runtime_error("Invalid encoded value for List [No Ending Delimiter 'e' found]: " + std::string(encoded_string));
        }

        while (encoded_string[pos] != 'e') {
            list.push_back(decodeBencode(encoded_string, pos));
            if (pos == std::string::npos) {
                throw std::runtime_error("Invalid encoded value for List [No Ending Delimiter 'e' found]: " + std::string(encoded_string));
            }
        }

        pos++;
        return list;

    }


    nlohmann::json decodeDict(const std::string_view &encoded_string, size_t &pos)
    {
        pos++;

        /**
         * @todo: nlohmann::json -> object or object_t
         */

        nlohmann::json dict = nlohmann::json::object();
        if (pos == std::string::npos) {
            throw std::runtime_error("Invalid encoded value for Dict [No Ending Delimiter 'e' found]: " + std::string(encoded_string));
        }

        while (encoded_string[pos] != 'e') {

            size_t end_keylen = encoded_string.find_first_of(':', pos);
            std::string key_len(encoded_string.substr(pos, end_keylen));
            end_keylen = key_len.find_first_of(':', 0);

            if (!std::isdigit(encoded_string[pos])) {
                throw std::runtime_error("Invalid encoded key value for Dict: " + key_len);
            }

            auto key = decodeString(encoded_string, pos);

            // check if pos == digit

            nlohmann::json val = (key == "pieces") ? decodePieces(encoded_string, pos) : decodeBencode(encoded_string, pos);
            dict[key] = val;
            
            // std::cout << "key: " << key.dump() << ", val: " << val.dump() << "\n\n";
        }

        /**
         * @todo: Check if keys are lexicographically sorted
         */

        pos++;
        
        return nlohmann::json(dict);

    }

    nlohmann::json decodePieces(const std::string_view& encoded_string, size_t& pos) {

        size_t colon_index = encoded_string.find_first_of(':', pos);

        /**
         * std::string npos
         * npos is a static member constant value with the greatest possible value for an element of type size_t.
         * As a return value, it is usually used to indicate no matches.
         * This constant is defined with a value of -1, which because size_t is an unsigned integral type, it is the largest possible representable value for this type.
         */

        if (colon_index != std::string_view::npos)
        {
            std::string len_str(encoded_string.substr(pos, colon_index - pos));
            uint64_t len_int = std::atoll(len_str.c_str());

            size_t size_str = static_cast<size_t>(len_int);

            if ((len_int == 0 && len_str.size() > 1) || (len_int > 0 && len_str[0] == '0')) {
                throw std::runtime_error("decodePieces: length of string trailing type: " + len_str);
            }

            size_t pos_end = colon_index + size_str;

            std::string decoded_res(encoded_string.substr(colon_index + 1, size_str));
            size_t decoded_len = decoded_res.size();

            if (decoded_len != size_str) {
                throw std::runtime_error("decodePieces: Invalid Input - string size doesn't match the following value");
            }

            pos = pos_end + 1;
            
            std::vector<unsigned char> data;
            data.reserve(size_str);
            for (auto byte: decoded_res) { data.push_back(static_cast<unsigned char>(byte)); };
            std::string result = bytesToHexString(data, size_str);
            return nlohmann::json(result);
        }
        else
        {
            std::string invalid_encoding(encoded_string);
            throw std::runtime_error("Invalid encoded value: " + invalid_encoding);
        }
    }

    std::string bytesToHexString(const std::vector<unsigned char>& data, size_t length) {
        std::ostringstream ss;
        ss << std::hex << std::uppercase << std::setfill('0');
        std::for_each(data.begin(), data.end(), [&](int byte) { ss << std::setw(2) << byte; });

        std::string result(ss.str());

        return result;     
    }





    bool isValidStrVal(std::string &str_val, size_t &pos, size_t &end) {
        
        std::string no_str(str_val.substr(pos, end - pos));
        bool is_valid = true;

        if (no_str[0] == '-') {
            return false;
        } else {
            is_valid = is_all_digits(no_str);
        }

        if (!is_valid) throw std::runtime_error("Is not all digit");

        int64_t no_val = std::stoll(no_str);

        if (no_str.length() > 1 && ((no_str[0] == '-' && no_str[1] == '0') || (no_str[0] == '0' && no_val >= 0)))
        {
            throw std::runtime_error("Invalid encoded value: " + no_str);
        }

        return true;

    }
    
    void encodeString(const nlohmann::json &json_obj, std::string &encoded_string) {


    }
    
    // [ ]: Unfinished Needs Proper Implementation for All Keys?

    // nlohmann::json decodeDict(const std::string &encoded_string, size_t &pos)
    // {

    //     // encoded_value starts with pos at: "d ... "

    //     pos++; // Move position to one char after 'd'

    //     nlohmann::json dict = nlohmann::json::object();

    //     while (encoded_string[pos] != 'e')
    //     {
    //         // Check whether the key type is string
    //         if (!std::isdigit(encoded_string[pos]))
    //         {
    //             throw std::runtime_error("Key cannot be a non-string value" + encoded_string);
    //         }

    //         auto key = decodeBencode(encoded_string, pos);

    //         // [ ]: Value for pieces - return type list of strings (improvement in infoTorrent())

    //         nlohmann::json val = (key == "pieces" || key == "peers") ? Bencode::piecestoHashStr(encoded_string, pos) : Bencode::decodeBencode(encoded_string, pos);

    //         // [ ]: Use map iterator method to insert (prevent unexpected addition of default value)

    //         dict[key] = val;
    //     }

    //     pos++;

    //     return nlohmann::json(dict);
    // }

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

    // auto encodeBencode(const nlohmann::json &json_obj, std::string &encoded_output) -> void
    // {

    //     if (json_obj.is_object())
    //     {
    //         encodeDict(json_obj, encoded_output);
    //     }
    //     else if (json_obj.is_array())
    //     {
    //         encodeList(json_obj, encoded_output);
    //     }
    //     else if (json_obj.is_string())
    //     {
    //         encodeStr(json_obj, encoded_output);
    //     }
    //     else if (json_obj.is_number())
    //     {
    //         encodeInt(json_obj, encoded_output);
    //     }
    //     else
    //     {
    //         throw std::runtime_error("json_obj passed is not of the type: object_j, array_j, string_j or number_j");
    //     }
    // }

    // auto encodeStr(const nlohmann::json &json_obj, std::string &encoded_output) -> void
    // {
    //     // Assuming json_obj is of the right type i.e. string_t

    //     // nolhmann::json using UTF-8 Encoding and std::string::size() / length()
    //     // returns number of bytes rather than characters in the string
    //     // Store in a Local String ? Or Divide size of string_json / 8 Bytes

    //     // json_obj.dump() -> Results in a string within a "double quotation"

    //     std::string local_string(json_obj.dump());
    //     local_string = local_string.substr(1, local_string.length() - 2);

    //     uint64_t str_len = local_string.length();
    //     encoded_output += std::to_string(str_len);
    //     encoded_output += ":";
    //     encoded_output += local_string;
    // }

    // auto encodeInt(const nlohmann::json &json_obj, std::string &encoded_output) -> void
    // {
    //     std::string string_number = json_obj.dump();
    //     encoded_output += "i" + string_number + "e";
    // }

    // auto encodeList(const nlohmann::json &json_obj, std::string &encoded_output) -> void
    // {
    //     encoded_output += "l";

    //     for (auto json_element : json_obj)
    //     {
    //         encodeBencode(json_element, encoded_output);
    //     }

    //     encoded_output += "e";
    // }

    // // nlohmann::json object_t (based on map or unordered_map
    // // Enforces Lexographical Ordering of Keys (Avoid Resorting)

    // auto encodeDict(const nlohmann::json &json_obj, std::string &encoded_output) -> void
    // {
    //     encoded_output += "d";
    //     for (auto it = json_obj.begin(); it != json_obj.end(); it++)
    //     {
    //         if (it.key() != "pieces")
    //         {
    //             encodeBencode(it.key(), encoded_output);   // Encode Key       // RECHECK: (Optimization) Break Chain & call string right away? Since key is always string?
    //             encodeBencode(it.value(), encoded_output); // Encode Value
    //         }
    //         else
    //         {
    //             encodeBencode(it.key(), encoded_output); // Encode Key       // RECHECK: (Optimization) Break Chain & call string right away? Since key is always string?
    //             std::string byte_string = hexToBytes(it.value());
    //             encoded_output += std::to_string(byte_string.length());
    //             encoded_output += ":";
    //             encoded_output += hexToBytes(it.value());
    //         }
    //     }

    //     encoded_output += "e";
    // }

    // // Helper Functions Hash - Hexadecimal Conversions

    // nlohmann::json piecesToHashStr(std::string::const_iterator &it_begin, std::string::const_iterator &it_end)
    // {
    //     auto it_colon = std::find(it_begin, it_end, ':');

    //     if (it_colon != it_end)
    //     {
    //         std::string number_string(it_begin, it_colon);
    //         u_int64_t string_length = std::atoll(number_string.c_str());
    //         std::string pieces_string(it_colon + 1, it_colon + string_length + 1);

    //         if ((string_length % 20 != 0) && (pieces_string.length() != string_length))   // [ ] Redundant check? pieces_string.size() != string_length
    //         {
    //             throw std::runtime_error("Invalid Length of pieces string: " + string_length);
    //         }

    //         std::string pieces_output;
    //         pieces_output.reserve(string_length * 2);

    //         pieces_output = bytesToHex(pieces_string);

    //         it_begin = it_colon + string_length + 1;

    //         return nlohmann::json(pieces_output);
    //     }
    //     else
    //     {
    //         std::string encoded_string(it_begin, it_end);
    //         throw std::runtime_error("Invalid encoded value: " + encoded_string);
    //     }

    // }

    // nlohmann::json piecestoHashStr(const std::string &encoded_string, size_t &pos)
    // {

    //     /**
    //      * @todo : Account for edge cases and exceptions
    //      *
    //      * Example :    "50i24e2:Hi"
    //      * Now Returns:  json: Hi - but an invalid bencoding
    //      *
    //      * Example :    "51:Hello"
    //      * Final Index:  2 - since in substr_next
    //      */

    //     size_t colon_index = encoded_string.find(':', pos);

    //     if (colon_index != std::string::npos)
    //     {
    //         std::string number_string = encoded_string.substr(pos, colon_index - pos);
    //         uint64_t number = std::atoll(number_string.c_str());

    //         std::string piece_str = encoded_string.substr(colon_index + 1, number);

    //         if ((number % 20 != 0) && (piece_str.size() != number))
    //         {
    //             throw std::runtime_error("Invalid Length of string pieces: " + number);
    //         }

    //         // Given the number of pieces covert to string for each 20 Bytes Piece

    //         auto piece_count = number / 20;

    //         // HACK: BytesToHex the entire piece str                                        (1)

    //         // pieces_output = Bencode::bytesToHex(piece_str);

    //         std::string pieces_output;
    //         pieces_output.reserve(number * 2);

    //         for (size_t i = 0; i < piece_count; ++i)
    //         {
    //             std::string piece_hash_binary = piece_str.substr(i * 20, 20);
    //             std::string piece_hash_hex = Bencode::bytesToHex(piece_hash_binary);
    //             pieces_output += piece_hash_hex;
    //         }

    //         pos = colon_index + number + 1;

    //         return nlohmann::json(pieces_output);
    //     }
    //     else
    //     {
    //         throw std::runtime_error("Invalid encoded value: " + encoded_string);
    //     }
    // }

    // auto bytesToHex(const std::string &bytes_string) -> std::string
    // {

    //     const unsigned char hex_digits[] = "0123456789abcdef";
    //     std::string output;

    //     output.reserve(bytes_string.length() * 2); // Each byte gets represented by 2 Hex

    //     for (unsigned char c : bytes_string)
    //     {
    //         output.push_back(hex_digits[c >> 4]);  // Get the first hex digit (higher nibble)
    //         output.push_back(hex_digits[c & 0xf]); // Get the second hex digit (lower nibble)
    //     }

    //     return output;
    // }

    // // LEARN: Simpler Implementation of Hexadecimal to Byte String

    // auto hexToBytes(const std::string &hex_string) -> std::string
    // {
    //     // Check Even Length
    //     if (hex_string.length() % 2 != 0)
    //     {
    //         throw std::invalid_argument("Hex String must have an even length");
    //     }

    //     std::string output;
    //     output.reserve(hex_string.length() / 2);

    //     for (size_t i = 0; i < hex_string.length(); i += 2)
    //     {
    //         char high_nibble = hex_string[i];
    //         char low_nibble = hex_string[i + 1];

    //         // Check chars valid hexadecimal digits
    //         if (!std::isxdigit(high_nibble) || !std::isxdigit(low_nibble))
    //         {
    //             // std::cout << "High Nibble Char: " << high_nibble << " Low Nibble Char: " << low_nibble << std::endl;
    //             throw std::invalid_argument("Hex String contains invalid characters.");
    //         }

    //         unsigned char byte = (std::stoi(std::string(1, high_nibble), nullptr, 16) << 4) |
    //                              std::stoi(std::string(1, low_nibble), nullptr, 16);

    //         output.push_back(static_cast<char>(byte));
    //     }

    //     return output;
    // }

    // // Torrent Parser

    // nlohmann::json parseTorrent(const std::string &path)
    // {
    //     std::streampos size;
    //     char *buffer;

    //     std::ifstream file(path, std::ios::in | std::ios::binary | std::ios::ate);
    //     nlohmann::json torrent_info;

    //     if (file.is_open())
    //     {
    //         size = file.tellg();
    //         int buff_size = file.tellg();
    //         buffer = new char[size];
    //         file.seekg(0, std::ios::beg);
    //         file.read(buffer, size);

    //         std::string torrent_info_str(buffer, buff_size);
    //         auto it_begin = torrent_info_str.cbegin();
    //         auto it_end = torrent_info_str.cend();

    //         try
    //         {
    //             // torrent_info = decodeEncoding(it_begin, it_end);
              
    //         }
    //         catch (...)
    //         {
    //             std::cout << "Exception in parse_torrent(): " << std::endl;
    //         }

    //         delete[] buffer;
    //         file.close();

    //         return torrent_info;
    //     }
    //     else
    //     {
    //         throw std::runtime_error("Unable to open file: " + path);
    //     }
    // }

    // const std::vector<std::string> getTorrentInfo(const std::string &path)  
    // {
    //     nlohmann::json torrent_table = parseTorrent(path);
    //     auto torrent_info = torrent_table.find("info"); // Return Type: Iterator

    //     std::string info_bencoded_str;
    //     Bencode::encodeBencode(*torrent_table.find("info"), info_bencoded_str);

    //     // sha1.hpp used here to calculate info hash

    //     SHA1 checksum;
    //     checksum.update(info_bencoded_str);
    //     std::string info_hash = checksum.final();

    //     // Retrieve Values for Keys:
    //     // NOTE: torrent_table on the stack - Dot Operator to access objects/elements
    //     // NOTE: torrent_info iterator type pointing to objects on the heap - Arrow Operator to access objects/elements

    //     auto tracker_it = torrent_table.find("announce");       // iterator to Tracker Link
    //     auto length_it = torrent_info->find("length");          // iterator to Length
    //     auto pieceLen_it = torrent_info->find("piece length");
    //     auto pieces_it = torrent_info->find("pieces");          // iterator to Pieces

    //     if (tracker_it != torrent_table.end() && length_it != torrent_info->end() && pieces_it != torrent_info->end() && pieceLen_it != torrent_info->end())
    //     {
    //         std::string link = (tracker_it.value()).dump();
    //         std::string length = (length_it.value()).dump();
    //         std::string piece_length = (pieceLen_it.value().dump());
    //         std::string pieces = (pieces_it.value()).dump();

    //         pieces = pieces.substr(pieces.find_first_of("\"") + 1, pieces.find_last_of("\"") -1);


    //         std::vector<std::string> vector_info;
    //         vector_info.push_back(link.substr(link.find_first_of("\"") + 1, link.find_last_of("\"") - 1));
    //         vector_info.push_back(length);
    //         vector_info.push_back(info_hash);
    //         vector_info.push_back(piece_length);

    //         // HACK: Hard-Coded no of pieces = 3, make it dynamic later

    //         for (size_t i = 0; i < pieces.size(); i+=40)
    //         {
    //             vector_info.push_back(pieces.substr(i, 40));
    //         }

    //         return vector_info;
    //     }

    //     return {};
    // }

}