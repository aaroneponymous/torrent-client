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

            nlohmann::json val = (key == "pieces" || key == "peers") ? decodePieces(encoded_string, pos) : decodeBencode(encoded_string, pos);
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
        ss << std::hex << std::setfill('0');
        std::for_each(data.begin(), data.end(), [&](int byte) { ss << std::setw(2) << byte; });

        std::string result(ss.str());

        return result;     
    }

    int getPieceLength(const nlohmann::json& json_obj) {

        auto it_info = *json_obj.find("info");
        auto val = it_info["piece length"];
        std::string piece_length(val.dump());
        return std::stoi(piece_length);
    }

    std::string getAnnounceURL(const nlohmann::json& json_obj) {
        auto announce_val = json_obj["announce"];
        std::string announce_url(announce_val.dump());
        announce_url.pop_back();
        announce_url.erase(announce_url.begin());
        return announce_url;
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

    void encodeBencode(const nlohmann::json &json_obj, std::string &encoded_string) {
        if (json_obj.is_string()) {
            encodeString(json_obj, encoded_string);
        }
        else if (json_obj.is_number_integer()) {
            encodeInteger(json_obj, encoded_string);
        }
        else if (json_obj.is_array()) {
            encodeList(json_obj, encoded_string);
        }
        else if (json_obj.is_object()) {
            encodeDict(json_obj, encoded_string);
        }
        else {
            throw std::runtime_error("Invalid Decoded Json");
        }
    }
    
    void encodeString(const nlohmann::json &json_obj, std::string &encoded_string) {

        if (!json_obj.is_string()) throw std::runtime_error("json_obj is not of type string");

        std::string decoded_str(json_obj.dump()); // is never empty : will always have "" quotations from dump()

        decoded_str.pop_back();
        char &first = decoded_str.front();
        first = ':';

        uint64_t str_len = decoded_str.length() - 1;
        encoded_string += std::to_string(str_len);
        encoded_string += decoded_str;

    }
    
    void encodeInteger(const nlohmann::json &json_obj, std::string &encoded_string) {

        if (!json_obj.is_number_integer()) throw std::runtime_error("json_obj not of type number integer");

        std::string int_val(json_obj.dump());
        encoded_string += "i" + int_val + "e";

    }

    void encodeList(const nlohmann::json &json_obj, std::string &encoded_string) {

        if (!json_obj.is_array()) throw std::runtime_error("json_obj not of type array");
        
        encoded_string.push_back('l');

        for (auto &obj: json_obj) { encodeBencode(obj, encoded_string); }

        encoded_string.push_back('e');
                

    }

    void encodeDict(const nlohmann::json &json_obj, std::string &encoded_string) {

        if (!json_obj.is_object()) throw std::runtime_error("json_obj not of type object");

        encoded_string.push_back('d');

        for (auto it = json_obj.begin(); it != json_obj.end(); it++)
        {
            if (it.key() != "pieces")
            {
                encodeBencode(it.key(), encoded_string);   // Encode Key       // RECHECK: (Optimization) Break Chain & call string right away? Since key is always string?
                encodeBencode(it.value(), encoded_string); // Encode Value
            }
            else
            {
                encodeBencode(it.key(), encoded_string); // Encode Key       // RECHECK: (Optimization) Break Chain & call string right away? Since key is always string?
                std::string byte_string = hexToBytes(it.value());
                encoded_string += std::to_string(byte_string.length());
                encoded_string.push_back(':');
                encoded_string += byte_string; 
            }
        }

        encoded_string.push_back('e');


    }

    unsigned char hexCharToByte(char ch) {
        if (ch >= '0' && ch <= '9') {
            return ch - '0';
        } else if (ch >= 'A' && ch <= 'F') {
            return ch - 'A' + 10;
        } else if (ch >= 'a' && ch <= 'f') {
            return ch - 'a' + 10;
        } else {
            throw std::runtime_error("Invalid hexadecimal character");
        }
    }

    std::string hexToBytes(const std::string &hexString) {
        if (hexString.length() % 2 != 0) {
            throw std::runtime_error("Hex string length must be even.");
        }

        std::vector<unsigned char> bytes;
        bytes.reserve(hexString.length() / 2); // Pre-allocate memory

        for (size_t i = 0; i < hexString.length(); i += 2) {
            unsigned char highNibble = hexCharToByte(hexString[i]);
            unsigned char lowNibble = hexCharToByte(hexString[i+1]);
            bytes.push_back((highNibble << 4) | lowNibble);
        }

        std::string byte_string(bytes.begin(), bytes.end());
        return byte_string;
    }

     std::string getInfoHash(const nlohmann::json &metadata_dict) {

        auto torrent_info = *metadata_dict.find("info");
        std::string info_bencoded("");
        encodeBencode(torrent_info,info_bencoded);

        SHA1 checksum;
        checksum.update(info_bencoded);
        return checksum.final();
    }

    std::vector<std::string> getPiecesHashed(const nlohmann::json &metadata_dict) {

        auto info_dict = *metadata_dict.find("info");
        std::string piecesHexString = *info_dict.find("pieces");
        int pieces_count = (piecesHexString.length())/(20 * 2);
        std::vector<std::string> hashed_pieces;
        hashed_pieces.reserve(pieces_count);


        for (int i = 0; i < pieces_count; i++) {
            
            std::string piece_hexed = piecesHexString.substr(i*20*2, 20*2);
            hashed_pieces.push_back(piece_hexed);
        
        }

        return hashed_pieces;
        
    }
   
    
    
    
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