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

// https://stackoverflow.com/questions/13905774/in-c-how-do-you-use-libcurl-to-read-a-http-response-into-a-string

struct URL_Data
{
    size_t size;
    char *data;
};

// nmemb - no of memory blocks or elements being passed to the function
// size  - the size (in bytes) of each individual element

size_t write_data(void *ptr, size_t size, size_t nmemb, struct URL_Data *url_data)
{
    size_t index = url_data->size;
    size_t n = (size * nmemb);
    char *temp;

    url_data->size += (size * nmemb);

    fprintf(stderr, "data at %p size=%ld nmemb=%ld\n", ptr, size, nmemb);

    temp = (char *)realloc(url_data->data, url_data->size + 1); /* +1 for nullable char '\0' */

    if (temp)
    {
        url_data->data = temp;
    }
    else
    {
        if (url_data->data)
        {
            free(url_data->data);
        }

        fprintf(stderr, "Failed to allocate memory.\n");
        return 0;
    }

    memcpy((url_data->data + index), ptr, n);
    url_data->data[url_data->size] = '\0';

    return size * nmemb;
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

        struct URL_Data data;
        data.size = 0;
        data.data = (char *)malloc(4096);
        if (NULL == data.data)
        {
            fprintf(stderr, "Failed to allocate memory.\n");
            return NULL;
        }

        data.data[0] = '\0';
        
        CURL *curl;
        CURLcode ret;


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

            curl_free(base_url);
            curl_free(info_hash_param1);
            curl_free(peer_id_param2);
            curl_free(port_no_param3);
            curl_free(uploaded_param4);
            curl_free(downloaded_param5);
            curl_free(compact_param7);

            curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

            // Perform the request
            ret = curl_easy_perform(curl);


            curl_easy_cleanup(curl);
            curl = nullptr;

            std::string response = data.data;
            size_t pos = 0;

            auto decoded_value = Bencode::decodeBencode(response, pos);

            std::cout << decoded_value.dump() << "\n";



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
