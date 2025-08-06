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

