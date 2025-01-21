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

    if (command == "decode")
    {
        if (argc < 3)
        {
            std::cerr << "Usage: " << argv[0] << " decode <encoded_value>" << std::endl;
            return 1;
        }

        std::string encoded_value = argv[2];
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
    else
    {
        std::cerr << "unknown command: " << command << std::endl;
        return 1;
    }

    return 0;
}
