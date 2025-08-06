/**
 * File: bendecoder.hpp
 *
 * Contains: Rudimentary implementation of decoding a bencoded data / file
 */

#pragma once

#include <string>
#include <iostream>
#include <vector>
#include <cctype>
#include <cstdlib>
#include <stdexcept>
#include <fstream>
#include <assert.h>
#include <algorithm>



#include "../json.hpp"
#include "../sha1.hpp"

namespace Bencode
{

    // Set of Decoding Functions

    nlohmann::json decodeBencode(const std::string_view &encoded_string, size_t &pos);

    nlohmann::json decodeString(const std::string_view &encoded_string, size_t &pos);

    nlohmann::json decodeInteger(const std::string_view &encoded_string, size_t &pos);

    nlohmann::json decodeList(const std::string_view &encoded_string, size_t &pos);

    nlohmann::json decodeDict(const std::string_view &encoded_string, size_t &pos);

    
    bool isValidStrVal(std::string &str_val, size_t &pos, size_t &end);


    // // Helper Functions Hash - Hexadecimal Conversions

    // nlohmann::json piecesToHashStr(std::string::const_iterator &it_begin, std::string::const_iterator &it_end);

    // nlohmann::json piecestoHashStr(const std::string &encoded_string, size_t &pos);

    // auto bytesToHex(const std::string &bytes_string) -> std::string;

    // auto hexToBytes(const std::string &hex_string) -> std::string;

    // // Set of Bencoding Functions

    // auto encodeBencode(const nlohmann::json &json_obj, std::string &encoded_output) -> void;

    // auto encodeStr(const nlohmann::json &json_obj, std::string &encoded_output) -> void;

    // auto encodeInt(const nlohmann::json &json_obj, std::string &encoded_output) -> void;

    // auto encodeList(const nlohmann::json &json_obj, std::string &encoded_output) -> void;

    // auto encodeDict(const nlohmann::json &json_obj, std::string &encoded_output) -> void;

    // // Torrent Parsers

    // nlohmann::json parseTorrent(const std::string &path);

    // const std::vector<std::string> getTorrentInfo(const std::string &path);


}