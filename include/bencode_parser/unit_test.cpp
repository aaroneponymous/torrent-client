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


    // std::string_view str_string("5:hello");
    // auto it_curr = str_string.cbegin();
    // auto it_end = str_string.cend();

    // parser.parse(str_string, it_curr, it_end);
    // std::cout << std::get<std::string_view>(parser.bencode_tree_.value_).data() << std::endl;

    std::vector<std::string_view> list_tests;

    // list of ints:    li1ei2ei3ee -> [1, 2, 3]
    std::string_view list_ints("li1ei2ei3ee");
    list_tests.push_back(list_ints);
    // list of mixed:   li42e4:spamee -> [42, "spam"]
    std::string_view mixed("li42e4:spamee");
    list_tests.push_back(mixed);
    // list of lists:   lli1ei2eei3ee -> [[1, 2], 3]
    std::string_view list_li("lli1ei2eei3ee");
    list_tests.push_back(list_li);
    // list of strings: l3:foo3:bar5:applee -> ["foo", "bar", "apple"]
    std::string_view list_str("l3:foo3:bar5:applee");
    list_tests.push_back(list_str);
    // list with dict:  ld3:foo3:bare4:spame -> [{"foo": "bar"}, "spam"]
    // malformed list:  li1ei2ei3e   ← missing final 'e'
    std::string_view malformed("li1ei2ei3e");
    list_tests.push_back(malformed);

    for (auto &test : list_tests)
    {
        auto it_curr = test.cbegin();
        auto it_end = test.cend();

        parser.parse(test, it_curr, it_end);
    }








    // std::string_view str_string("")
   

    return 0;

}