#include "../include/bittorrent/bencode.hpp"
#include <sstream>
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
        if (argc < 3)
        {
            std::cerr << "Usage: " << argv[0] << " info path/to/file.torrent" << std::endl;
        }

        std::streampos size;
        char *buffer;

        std::string path(argv[2]);

        std::ifstream file(path, std::ios::in | std::ios::binary | std::ios::ate);
        nlohmann::json torrent_info;

        if (file.is_open())
        {
            size = file.tellg();
            int buff_size = file.tellg();
            buffer = new char[size];
            file.seekg(0, std::ios::beg);
            file.read(buffer, size);

            std::string_view torrent_info_str(buffer, buff_size);
            size_t pos = 0;

            try
            {
                torrent_info = Bencode::decodeBencode(torrent_info_str, pos);
                std::cout << torrent_info.dump() << "\n";
              
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
    else if (command == "test_str")
    {
        if (argc < 3)
        {
            std::cerr << "Usage: " << argv[0] << " testStr <encoded_value>" << std::endl;
            return 1;
        }

        std::string_view encoded_value(argv[2]);
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
            // Wrap the line buffer in a string_view:
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
    else if (command == "testList")
    {

    }
    else if (command == "testDict")
    {

    }
    else
    {
        std::cerr << "unknown command: " << command << std::endl;
        return 1;
    }

    return 0;
}

