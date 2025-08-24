#include "../include/bittorrent/bencode.hpp"
#include "../include/bittorrent/tracker.hpp"
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <set>
#include <random>


struct MemoryBuffer {
    std::string data;
};


static size_t writeCallBack(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    auto* mem = static_cast<MemoryBuffer*>(userp);
    mem->data.append(static_cast<char*>(contents), realsize);
    return realsize;
}

// std::string generatePeerID() {
//     std::random_device rand_dev;
//     std::uniform_int_distribution<int> dist(0, 9);

//     std::string peer_id;
//     for (int i = 0; i < 20; ++i) {
//         int rand = dist(rand_dev);  // rand is 0-9
//         char rand_ch = static_cast<char>(rand); // char(rand) converts rand to \x00, \x01 .. (ASCII control char)
//         peer_id.push_back(rand_ch);
//         std::cout << rand_ch;
//     }

//     return peer_id;
// }


std::string generatePeerID() {
    std::random_device rand_dev;
    std::uniform_int_distribution<int> dist(0, 9);

    std::string peer_id;
    peer_id.reserve(20);

    for (int i = 0; i < 20; ++i) {
        int rand = dist(rand_dev);
        char rand_ch = '0' + static_cast<char>(rand);
        peer_id.push_back(rand_ch);
    }

    return peer_id;
}

std::string getTrackerResponse(std::string& announce_url, std::string& info_hash, std::string& peer_id, int port,
                               int uploaded, int downloaded, int left, int compact) {

    CURL *curl_handle;
    CURLcode res;
    
    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();

    std::string url_infoHash = Tracker::urlEncode(info_hash, curl_handle);
    std::string url_peerID = Tracker::urlEncode(peer_id, curl_handle);

    std::string url("");
    url.append(announce_url);
    url.append("?info_hash=" + url_infoHash);
    url.append("&peer_id=" + url_peerID);
    url.append("&port=" + std::to_string(port));
    url.append("&uploaded=" + std::to_string(uploaded));
    url.append("&downloaded=" + std::to_string(downloaded));
    url.append("&left=" + std::to_string(left));
    url.append("&compact=" + std::to_string(compact));

    std::cout << url << std::endl;

    struct MemoryBuffer mem_buff;
    curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, writeCallBack);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, static_cast<void*>(&mem_buff));
    res = curl_easy_perform(curl_handle);
    
    if(res != CURLE_OK) {
        std::cout << "curl_easy_perform() failed: " << curl_easy_strerror(res) <<  std::endl;
    }
    else {
        std::cout << "Bytes Received: " << mem_buff.data.size() << std::endl;
    }

    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();

    return mem_buff.data;

}



int main(int argc, char *argv[])
{
    // Flush after every std::cout / std::cerr

    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;


    std::set<std::string> commands({"test_str", "test_int", "test_list", "test_dict"});

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


    }
    else if (command == "peer_id") {
        std::string peer_id = generatePeerID();
        std::cout << peer_id << std::endl;
    }
    else if (command == "info")
    {
        if (argc < 3) {
                std::cerr << "Usage: " << argv[0] << " info path/to/file.torrent\n";
                return 1;
        }

        std::string path{ argv[2] };
        std::ifstream file{ path, std::ios::binary };
        if (!file)
        {
            std::cerr << "Error: could not open file `" << path << "`\n";
            return 1;
        }

        // Read entire file into a string
        std::ostringstream oss;
        oss << file.rdbuf();
        std::string file_contents = oss.str();

        // Wrap in string_view (safe because file_contents outlives view)
        std::string_view torrent_sv{ file_contents };

        size_t pos = 0;
        nlohmann::json torrent_info;
        try
        {
            torrent_info = Bencode::decodeBencode(torrent_sv, pos);
            std::cout << torrent_info << "\n";
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error parsing torrent: " << e.what() << "\n";
            return 1;
        }

        std::string encode_output("");

        Bencode::encodeBencode(torrent_info, encode_output);

        std::string announce_url = Bencode::getAnnounceURL(torrent_info);
        std::string info_hash = Bencode::getInfoHash(torrent_info);
        std::string info_bytes = Bencode::hexToBytes(info_hash);
        std::string peer_id = generatePeerID();
        int piece_length = Bencode::getPieceLength(torrent_info);

        std::vector<std::string> hashed_pieces = Bencode::getPiecesHashed(torrent_info);

        int bytes_left = piece_length * hashed_pieces.size();

        std::string tracker_response = getTrackerResponse(announce_url, info_bytes, peer_id,
                                                          6881, 0, 0, bytes_left, 1);
        size_t pos_t = 0;

        std::cout << tracker_response << std::endl;

        nlohmann::json response_info = Bencode::decodeBencode(tracker_response, pos_t);
        std::cout << response_info.dump() << std::endl;



        

    }
    else if (command == "test_str")
    {
        if (argc < 3)
        {
            std::cerr << "Usage: " << argv[0] << " testStr <encoded_value>" << std::endl;
            return 1;
        }

        std::string contents(argv[2]);
        std::string_view encoded_value{contents};

        size_t pos = 0;
        nlohmann::json decoded_value = Bencode::decodeString(encoded_value, pos);
        std::cout << decoded_value.dump() << "\n";

    }
    else if (commands.contains(command))
    {
        std::ifstream file{ argv[2] };
        if (!file)
        {
            std::cerr << "Error: could not open file `" << argv[2] << "`\n";
            return 1;
        }

        std::string line;
        while (std::getline(file, line))
        {
            std::string_view encoded_value{ line };

            size_t pos = 0;

            try
            {
                nlohmann::json decoded = Bencode::decodeBencode(encoded_value, pos);
                std::cout << decoded.dump() << "\n";
            }
            catch (const std::exception &e)
            {
                if (command == "test_int") {
                    std::cerr << "decodeInteger error on `" << line << "`: " << e.what() << "\n";
                } else if (command == "test_str") {
                    std::cerr << "decodeString error on `" << line << "`: " << e.what() << "\n";
                } else if (command == "test_list") {
                    std::cerr << "decodeList error on `" << line << "`: " << e.what() << "\n";
                } else if (command == "test_dict") {
                    std::cerr << "decodeDict error on `" << line << "`: " << e.what() << "\n";
                } else {
                    std::cerr << "Wrong Command Input" << "\n";
                }

            }
        }

    }
    else if (command == "encodeString")
    {
        const nlohmann::json empty("");
        const nlohmann::json non_empty("hello");
        std::string output("");

        Bencode::encodeString(empty, output);
        std::cout << "Empty: " << output << "\n";
        output.clear();
        Bencode::encodeString(non_empty, output);
        std::cout << "Non-Empty: " << output << "\n";


    }
    else if (command == "encodeInteger")
    {
        const nlohmann::json val(50);
        std::string output("");
        Bencode::encodeInteger(val, output);
        std::cout << output << "\n";

    }
    else if (command == "encodeList")
    {
        const nlohmann::json array_strings({"hello", "world"});
        std::string output("");
        Bencode::encodeList(array_strings, output);
        std::cout << output << "\n";

    }
    else
    {
        
        std::cerr << "unknown command: " << command << std::endl;
        return 1;
    }

    return 0;
}

