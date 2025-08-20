#include "../include/bittorrent/bencode.hpp"
#include <sstream>
#include <stdio.h>
#include <set>


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

        // std::string encode_output("");

        // Bencode::encodeBencode(torrent_info, encode_output);
        // std::cout << "\n\nEncoded Output: " << encode_output << "\n";

        std::string info_hash = Bencode::getInfoHash(torrent_info);

        std::cout << "Info Hash: " << info_hash << "\n";

        std::vector<std::string> hashed_pieces = Bencode::getPiecesHashed(torrent_info);

        std::cout << "Hashed Pieces: " << "\n";
        for (auto &piece_hash : hashed_pieces) {
            std::cout << piece_hash << "\n";
        }


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

