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
#include <sstream>
#include <iomanip>
#include <algorithm>



#include "../json.hpp"
#include "../sha1.hpp"

namespace Bencode
{

    // Set of Decoding Functions

    nlohmann::json decodeBencode(const std::string_view& encoded_string, size_t& pos);

    nlohmann::json decodeString(const std::string_view& encoded_string, size_t& pos);

    nlohmann::json decodeInteger(const std::string_view& encoded_string, size_t& pos);

    nlohmann::json decodeList(const std::string_view& encoded_string, size_t& pos);

    /**
     * @todo: object_t():
     * 
     * Storage:
     * Objects are stored as pointers in a basic_json type.
     * That is, for any access to object values, a pointer of
     * type object_t* must be dereferenced.
     * 
     * Object key order:
     * The order name/value pairs are added to the object 
     * are not preserved by the library. Therefore, 
     * iterating an object may return name/value pairs in a 
     * different order than they were originally stored. 
     * In fact, keys will be traversed in alphabetical order 
     * as std::map with std::less is used by default. 
     * Please note this behavior conforms to RFC 8259, because 
     * any order implements the specified "unordered" nature of JSON objects.
     */
    

    nlohmann::json decodeDict(const std::string_view& encoded_string, size_t& pos);

    nlohmann::json decodePieces(const std::string_view& encoded_string, size_t& pos);

    std::string bytesToHexString(const std::vector<unsigned char>& data, size_t length);
    
    bool isValidStrVal(std::string& str_val, size_t& pos, size_t& end);

    /**
     * Consideration for Info Hash: https://bittorrent.org/beps/bep_0003.html
     * 
     * 
     * info_hash:
     * The 20 byte sha1 hash of the bencoded form of the info value from the metainfo file.
     * This value will almost certainly have to be escaped.
     * 
     * Note that this is a substring of the metainfo file.
     * 
     * The info-hash must be the hash of the encoded form as found in the .torrent file, 
     * which is identical to bdecoding the metainfo file, extracting the info dictionary and 
     * encoding it if and only if the bdecoder fully validated the input (e.g. key ordering, absence of leading zeros). 
     * Conversely that means clients must either reject invalid metainfo files or extract the substring directly. 
     * 
     * They must not perform a decode-encode roundtrip on invalid data.
     */

    // Set of Encoding Functions

    /**
     * Encoding to Bencode
     * 
     * - Final Outcome: Generate SHA-1 hash of bencoded "info" dict
     * - Is dictinary implementation of nlohmann::json ordered or unordered? Should be ordered? Lexicographical Ordering of Keys?
     * - Intermediate Outcome: Return bencoded string of "info" dict
     * - Pass empty string by reference down the recursion? Simple push_back()?
     * - 
    **/

    void encodeInfoDict(const nlohmann::json &json_object, std::string &encoded_output);

    void encodeBencode(const nlohmann::json &json_object, std::string &encoded_output);

    void encodeString(const nlohmann::json &json_object, std::string &encoded_output);
    
    void encodeInteger(const nlohmann::json &json_object, std::string &encoded_output);

    void encodeList(const nlohmann::json &json_object, std::string &encoded_output);

    void encodeDict(const nlohmann::json &json_object, std::string &encoded_output);

    std::string hexToBytes(const std::string &hex_string);

    std::string getInfoHash(const nlohmann::json &metadata_dict);

    std::vector<std::string> getPiecesHashed(const nlohmann::json &metadata_dict);

    int getPieceLength(const nlohmann::json& json_obj);

    std::string getAnnounceURL(const nlohmann::json& json_obj);


}