    #include <iostream>
    #include <string>
    #include <vector>
    #include <cctype>
    #include <cstdlib>
    #include <stdexcept>
    #include <fstream>

    #include "lib/json/json.hpp"

    using json = nlohmann::json;

    // Forward Declaration

    auto decode_bencoded_value(const std::string &encoded_value, size_t &pos) -> json;

    auto decode_str_value(const std::string &encoded_value, size_t &pos) -> json;

    auto bytes_to_hex(const std::string &input) -> std::string
    {https://status.codecrafters.io
        static const char hex_digits[] = "0123456789abcdef";
        std::string output;

        output.reserve(input.length() * 2); // Each byte gets represented by 2 Hex

        for (unsigned char c : input)
        {
            output.push_back(hex_digits[c >> 4]);  // Get the first hex digit (higher nibble)
            output.push_back(hex_digits[c & 0xf]); // Get the second hex digit (lower nibble)
        }

        return output;
    }

    auto decode_str_value_hex(const std::string &encoded_value, size_t &pos) -> json
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

        size_t colon_index = encoded_value.find(':', pos);

        if (colon_index != std::string::npos)
        {
            std::string number_string = encoded_value.substr(pos, colon_index - pos);
            int64_t number = std::atoll(number_string.c_str());

            std::string str = encoded_value.substr(colon_index + 1, number);
            pos = colon_index + number + 1;

            // if (str.length() == 20) // Likely a SHA-1 hash
            // {
            //     return json(bytes_to_hex(str)); // Convert to hex before returning
            // }

            return json(bytes_to_hex(str));
        }
        else
        {
            throw std::runtime_error("Invalid encoded value: " + encoded_value);
        }
    }

    auto decode_dict_value(const std::string &encoded_value, size_t &pos) -> json
    {
        // encoded_value starts with pos at: "d ... "

        pos++; // Move position to one char after 'd'

        json dict = json::object();

        while (encoded_value[pos] != 'e')
        {
            auto key = decode_bencoded_value(encoded_value, pos);

            json val;

            if (key == "pieces")
            {
                val = decode_str_value_hex(encoded_value, pos);
            }
            else
            {
                val = decode_bencoded_value(encoded_value, pos);
            }
            

            // Unnecessary creation of temporary pair - remove
            // auto [key, val] = std::pair{decode_bencoded_value(encoded_value, pos), decode_bencoded_value(encoded_value, pos)};
            
            dict[key] = val;
        }

        pos++;

        return json(dict);
    }

    auto decode_list_value(const std::string &encoded_value, size_t &pos) -> json
    {
        pos++;

        json list = json::array();

        while (encoded_value[pos] != 'e')
        {
            // std::cout << "List Substring " << encoded_value.substr(pos) + "\n";
            list.push_back(decode_bencoded_value(encoded_value, pos));
        }

        pos++;

        return list;
    }

    auto decode_int_value(const std::string &encoded_value, size_t &pos) -> json
    {

        size_t e_index = encoded_value.find_first_of('e', pos);

        if (e_index != std::string::npos)
        {
            // Check for disallowed values
            std::string number_str = encoded_value.substr(pos + 1, e_index - 1);
            int64_t number = std::atoll(number_str.c_str());

            // Account for valid integer str conversions
            // Invalid: i-0e or trailing zeroes i002e

            // std::cout << "number_str: " + number_str + "\n";
            // std::cout << "number_str length: " << number_str.length() << "\n";

            if (number_str.length() > 1 && ((number_str[0] == '-' && number == 0) || (number_str[0] == '0' && number >= 0)))
            {
                throw std::runtime_error("Invalid encoded value: " + encoded_value);
            }

            pos = e_index + 1;
            return json(number);
        }
        else
        {
            throw std::runtime_error("Invalid encoded value: " + encoded_value);
        }
    }

    auto decode_str_value(const std::string &encoded_value, size_t &pos) -> json
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

        size_t colon_index = encoded_value.find(':', pos);

        if (colon_index != std::string::npos)
        {
            std::string number_string = encoded_value.substr(pos, colon_index - pos);
            int64_t number = std::atoll(number_string.c_str());

            std::string str = encoded_value.substr(colon_index + 1, number);
            pos = colon_index + number + 1;

            return json(str);
        }
        else
        {
            throw std::runtime_error("Invalid encoded value: " + encoded_value);
        }
    }

    auto decode_bencoded_value(const std::string &encoded_value, size_t &pos) -> json
    {
        // std::cout << "decode_bencoded_value: " + encoded_value.substr(pos) << std::endl;

        if (std::isdigit(encoded_value[pos])) // Case: Char at [pos] is digit (string)
        {
            return decode_str_value(encoded_value, pos);
        }
        else if (encoded_value[pos] == 'i') // Case: Char at [pos] is 'i' (integer)
        {
            return decode_int_value(encoded_value, pos);
        }
        else if (encoded_value[pos] == 'l') // Case: Char at [pos] is 'l' (list)
        {
            return decode_list_value(encoded_value, pos);
        }
        else if (encoded_value[pos] == 'd')
        {
            return decode_dict_value(encoded_value, pos);
        }
        else
        {
            throw std::runtime_error("Unhandled encoded value: " + encoded_value);
        }
    }

    auto parse_torrent(const std::string &path, size_t &pos) -> json
    {
        // std::streampos size;
        std::streampos size;
        char *buffer;

        std::ifstream file(path, std::ios::in | std::ios::binary | std::ios::ate);
        json torrent_info;

        if (file.is_open())
        {

            size = file.tellg();
            int buff_size = file.tellg();
            buffer = new char[size];
            file.seekg(0, std::ios::beg);
            file.read(buffer, size);

            json torrent_info;
            std::string torrent_info_str(buffer, buff_size);

            try
            {
                size_t position = 0;
                torrent_info = decode_bencoded_value(torrent_info_str, position);
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

    auto get_info(const std::string &path, size_t &pos) -> std::vector<std::string>
    {
        json torrent_table = parse_torrent(path, pos);
        auto torrent_info = torrent_table.find("info");

        // json info_string <- Extract the Tracker Link and Size 
        auto tracker_it = torrent_table.find("announce");
        auto size_it = torrent_info->find("length");
        
        if (tracker_it != torrent_table.end() && size_it != torrent_info->end())
        {
            json link_j = tracker_it.value();
            json length_j = size_it.value();

            std::string link = link_j.dump();
            std::string length = length_j.dump();

            std::vector<std::string> output = {link.substr(1, link.length() - 2), length};
            return output;
        }

        return {};
        
    }

    // auto info_torrent(const std::string &path, size_t &pos) -> std::pair<json, json>
    // {
    //     json::object_t torrent_info = parse_torrent(path, pos);

    //     json::string_t key_url = "announce";
    //     json::string_t key_info = "info";
    //     json::string_t key_size = "piece length";

    //     json::string_t url = torrent_info.find(key_url)->second;
    //     json::object_t info_map = torrent_info.find(key_info)->second;
    //     json::string_t piece = info_map.find(key_size)->second;
        
    //     return std::make_pair(json(url), json(piece));
    // }

    int main(int argc, char *argv[])
    {
        // Flush after every std::cout / std::cerr
        std::cout << std::unitbuf;
        std::cerr << std::unitbuf;

        if (argc < 2)
        {
            std::cerr << "Usage: " << argv[0] << " decode <encoded_value>" << std::endl;
            return 1;
        }

        std::string command = argv[1];

        if (command == "decode")
        {
            if (argc < 3)
            {
                std::cerr << "Usage: " << argv[0] << " decode <encoded_value>" << std::endl;
                return 1;
            }

            std::string encoded_value = argv[2];
            size_t position = 0;
            json decoded_value = decode_bencoded_value(encoded_value, position);
            std::cout << decoded_value.dump() << std::endl;
        }
        else if (command == "info")
        {
            if (argc < 3)
            {
                std::cerr << "Usage: " << argv[0] << " info path/to/file.torrent" << std::endl;
            }

            std::string path = argv[2];
            size_t pos = 0;
            std::vector<std::string> decoded_torrent_info = get_info(path, pos);

            std::cout << "Tracker URL: " + decoded_torrent_info[0] + "\n";
            std::cout << "Length: " + decoded_torrent_info[1] << std::endl;
            // std::string output = decoded_torrent_info.dump();
            //     std::cout << output.substr(1, output.size() - 2) << std::endl;
            // std::cout << std::to_string(decoded_torrent_info) << std::endl;
            // std::cout << decoded_torrent_info.dump() << std::endl;

            // std::pair<json, json> tor(parse_torrent(path, pos));
            // std::cout << "Tracker URL: " << tor.first.dump() << "\n";
            // std::cout << "Piece Length: " << tor.second.dump() << std::endl;
        }
        else
        {
            std::cerr << "unknown command: " << command << std::endl;
            return 1;
        }

        return 0;
    }
