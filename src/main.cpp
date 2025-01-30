#include <iostream>
#include "../include/bittorrent/bencode.hpp"
#include <stdio.h>
#include <curl/curl.h>
#include <random>

std::string generateRandomPeerID(int length)
{
    const std::string characters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<int> distribution(0, characters.size() - 1);

    std::string random_string;
    for (auto i = 0; i < length; ++i)
    {
        random_string += characters[distribution(generator)];
    }

    return random_string;
}

static size_t WriteCallback(char *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    std::cout.write(contents, realsize);
    return realsize;
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
    const std::string encoded_value(argv[2]);
    auto it_begin = encoded_value.cbegin();
    auto it_end = encoded_value.cend();

    if (command == "decode")
    {
        if (argc < 3)
        {
            std::cerr << "Usage: " << argv[0] << " decode <encoded_value>" << std::endl;
            return 1;
        }

        size_t position = 0;
        auto decoded_value = Bencode::decodeBencode(encoded_value, position);
        std::cout << decoded_value.dump() << std::endl;
    }
    else if (command == "info")
    {
        if (argc < 3)
        {
            std::cerr << "Usage: " << argv[0] << " info path/to/file.torrent" << std::endl;
        }

        std::string path = argv[2];

        std::vector<std::string> decoded_torrent_info = Bencode::getTorrentInfo(path);

        std::cout << "Tracker URL: " + decoded_torrent_info[0] + "\n";
        std::cout << "Length: " + decoded_torrent_info[1] + "\n";
        std::cout << "Info Hash: " + decoded_torrent_info[2] + "\n";
        std::cout << "Piece Length: " + decoded_torrent_info[3] + "\n";
        std::cout << "Piece Hashes: \n";
        for (auto i = 4; i < decoded_torrent_info.size(); ++i)
        {
            std::cout << decoded_torrent_info[i] << "\n";
        }

        std::string url = decoded_torrent_info[0];
        std::string info_hash = decoded_torrent_info[2];
        std::string total_length = decoded_torrent_info[1];

        // Curl Methods & Calls

        CURLcode ret;
        CURL *curl;

        curl = curl_easy_init();

        if (curl)
        {
            char *base_url = curl_easy_escape(curl, url.c_str(), 0); 
            char *info_hash_param1 = curl_easy_escape(curl, Bencode::hexToBytes(info_hash).c_str(), 0);
            char *peer_id_param2 = curl_easy_escape(curl, generateRandomPeerID(20).c_str(), 0);
            char *port_no_param3 = curl_easy_escape(curl, "6881", 0);
            char *uploaded_param4 = curl_easy_escape(curl, "0", 0);
            char *downloaded_param5 = curl_easy_escape(curl, "0", 0);
            char *left_param6 = curl_easy_escape(curl, total_length.c_str(), 0);
            char *compact_param7 = curl_easy_escape(curl, "1", 0);

            std::string full_url = url + "?info_hash=" + info_hash_param1 + "&peer_id=" + peer_id_param2 + "&port=" + port_no_param3 + "&uploaded=" + uploaded_param4 + "&downloaded=" + downloaded_param5 + "&left=" + left_param6 + "&compact=" + compact_param7;
            std::cout << full_url << "\n";

            curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());

            // Set the write callback
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

            // Perform the request
            ret = curl_easy_perform(curl);

            curl_easy_cleanup(curl);
            curl = nullptr;

            std::cout << static_cast<int>(ret) << std::endl;
        }
    }
    else if (command == "testStr")
    {
        if (argc < 3)
        {
            std::cerr << "Usage: " << argv[0] << " testStr <encoded_value>" << std::endl;
            return 1;
        }

        Bencode::decodeString(it_begin, it_end);
    }
    else if (command == "testInt")
    {

        Bencode::decodeInteger(it_begin, it_end);
    }
    else if (command == "testList")
    {
        auto decoded_list = Bencode::decodeListing(it_begin, it_end);
        std::cout << decoded_list.dump() << std::endl;
    }
    else if (command == "testDict")
    {
        auto decoded_dict = Bencode::decodeEncoding(it_begin, it_end);
        std::cout << decoded_dict.dump() << std::endl;
    }
    else
    {
        std::cerr << "unknown command: " << command << std::endl;
        return 1;
    }

    return 0;
}
