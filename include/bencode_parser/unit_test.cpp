#include "bencode_parser.hpp"

using namespace Bencode;

int main()
{
    Parser parser = Parser();
    // std::string_view int_string("i245e");
    // auto it_curr = int_string.cbegin();
    // auto it_end = int_string.cend();


    // parser.parse(int_string, it_curr, it_end);
    // std::cout << std::get<int64_t>(parser.bencode_tree_.value_) << std::endl;

    // std::string str_length("24");
    // auto ul_length = std::strtoull(str_length.c_str(), nullptr, 10);
    // std::cout << ul_length << std::endl;


    std::string_view str_string("5:hello");
    auto it_curr = str_string.cbegin();
    auto it_end = str_string.cend();

    parser.parse(str_string, it_curr, it_end);
    std::cout << std::get<std::string_view>(parser.bencode_tree_.value_).data() << std::endl;
   

    return 0;

}