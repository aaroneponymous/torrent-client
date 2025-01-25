#include <iostream>
#include "../include/bittorrent/bencode.hpp"


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

        size_t pos = 0;
        std::vector<std::string> decoded_torrent_info = Bencode::infoTorrent(path, pos);

        std::cout << "Tracker URL: " + decoded_torrent_info[0] + "\n";
        std::cout << "Length: " + decoded_torrent_info[1] + "\n";
        std::cout << "Info Hash: " + decoded_torrent_info[2] + "\n";
        std::cout << "Piece Length: " + decoded_torrent_info[3] + "\n";
        std::cout << "Piece Hashes: \n";
        for (auto i = 4; i < decoded_torrent_info.size(); ++i)
        {
            std::cout << decoded_torrent_info[i] << "\n";
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
