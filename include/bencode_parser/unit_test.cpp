#include "bencode_parser.hpp"

using namespace Bencode;

int main()
{
    Parser parser = Parser();
    std::string_view int_string("i245e");
    auto it_curr = int_string.cbegin();
    auto it_end = int_string.cend();


    // Parser::BencodeValue val = parser.parse_stream(int_string, it_curr, it_end);
    // std::cout << std::get<int64_t>(val.value_) << std::endl;

    parser.parse(int_string, it_curr, it_end);
    std::cout << std::get<int64_t>(parser.bencode_tree_.value_) << std::endl;
   

    return 0;

}