#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include "../bencode.hpp"

namespace fs = std::filesystem;

std::vector<std::string> loadTestCases(const fs::path &filepath) {
    std::vector<std::string> cases;
    std::ifstream file(filepath);
    if (!file) {
        std::cerr << "Could not open test file: " << filepath << "\n";
        return cases;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        cases.push_back(line);
    }
    return cases;
}

int main() {
    int total = 0;
    int passed = 0;

    // (same folder as this source file)
    fs::path tests_dir = fs::path(__FILE__).parent_path();

    for (const auto &entry : fs::directory_iterator(tests_dir)) {
        if (entry.is_regular_file() &&
            entry.path().filename().string().find("_tests.txt") != std::string::npos) {
            
            std::cout << "\nRunning tests from: " << entry.path().filename() << "\n";

            auto cases = loadTestCases(entry.path());
            for (auto &input : cases) {
                total++;
                try {
                    auto root = bencode::BencodeParser::parse(input);
                    std::cout << "  ✔ Passed: " << input << ": " << root.toString() << "\n";
                    
                    (void)root; // unused for now
                    passed++;

                } catch (const std::exception &ex) {
                    std::cout << "  ✘ Failed: " << input 
                              << " | Exception: " << ex.what() << "\n";
                } catch (...) {
                    std::cout << "  ✘ Failed: " << input 
                              << " | Unknown exception\n";
                }
            }
        }
    }

    std::cout << "\nSummary: " << passed << "/" << total << " tests passed.\n";

    // Return non-zero if failures so CTest marks it failed
    return (passed == total) ? 0 : 1;
}
