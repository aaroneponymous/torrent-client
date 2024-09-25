#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <cstdlib>
#include <algorithm>

#include "lib/json/json.hpp"

using json = nlohmann::json;

// Forward Declaration

auto decode_bencoded_value(const std::string &encoded_value, size_t &pos) -> json;

auto decode_list_value(const std::string &encoded_value, size_t &pos) -> json
{
    pos++;

    json list = json::array();

    while (encoded_value[pos] != 'e')
    {
        std::cout << "List Substring " << encoded_value.substr(pos) + "\n";

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
        std::cout << "number of string " << number_string + "\n";
        int64_t number = std::atoll(number_string.c_str());
        std::string str = encoded_value.substr(colon_index + 1, number);
        std::cout << "str word " << str + "\n";

        pos = colon_index + number + 1;


        return json(str);
    }
    else
    {
        throw std::runtime_error("Invalid encoded value: " + encoded_value);
    }
}


json decode_bencoded_value(const std::string &encoded_value, size_t &pos)
{
    std::cout << "decode_bencoded_value: " + encoded_value.substr(pos) << std::endl;

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
    else
    {
        throw std::runtime_error("Unhandled encoded value: " + encoded_value);
    }
}

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
        // You can use print statements as follows for debugging, they'll be visible when running tests.
        // std::cout << "Logs from your program will appear here!" << std::endl;

        // Uncomment this block to pass the first stage
        std::string encoded_value = argv[2];
        size_t position = 0;
        json decoded_value = decode_bencoded_value(encoded_value, position);
        std::cout << decoded_value.dump() << std::endl;
    }
    else
    {
        std::cerr << "unknown command: " << command << std::endl;
        return 1;
    }

    return 0;
}
