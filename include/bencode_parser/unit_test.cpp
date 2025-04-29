#include "bencode_parser.hpp"

using namespace Bencode;

int main()
{
    Parser parser = Parser();
    std::string_view int_string("i245e");
    auto it_curr = int_string.cbegin();
    auto it_end = int_string.cend();

    parser.parse(int_string, it_curr, it_end);
    Parser::BencodeItem bencode_node = parser.bencode_tree_;

    std::cout << std::get<int64_t>(bencode_node.value_) << std::endl;

    return 0;

}