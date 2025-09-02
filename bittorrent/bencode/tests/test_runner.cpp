#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <optional>

#include "../bencode.hpp"

namespace fs = std::filesystem;

static std::vector<std::string> loadLines(const fs::path &filepath) {

    std::vector<std::string> lines;
    std::ifstream file(filepath);

    if (!file) {
        std::cerr << "Could not open test file: " << filepath << "\n";
        return lines;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        if (line[0] == '#') continue;
        lines.push_back(line);
    }
    
    return lines;
}

static std::string hexlify(std::string_view sv) {

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    for (unsigned char c : std::string(sv)) {
        oss << std::setw(2) << static_cast<int>(c);
    }

    return oss.str();
}

static std::string trim(std::string s) {

    auto issp = [](unsigned char c){ return std::isspace(c); };

    while (!s.empty() && issp(s.front())) s.erase(s.begin());
    while (!s.empty() && issp(s.back()))  s.pop_back();

    return s;
}

int main() {

    int total = 0;
    int passed = 0;

    fs::path tests_dir = fs::path(__FILE__).parent_path();

    // -----------------------------
    // 1) *_tests.txt
    // -----------------------------
    for (const auto &entry : fs::directory_iterator(tests_dir)) {

        if (!entry.is_regular_file()) continue;
        const auto fname = entry.path().filename().string();

        if (fname.find("_tests.txt") == std::string::npos ||
            fname.find("_infoslice_tests.txt") != std::string::npos) {
            continue;
        }

        std::cout << "\nRunning parser tests from: " << entry.path().filename() << "\n";
        auto cases = loadLines(entry.path());
        for (auto &input : cases) {
            total++;

            try {
                auto root = bencode::BencodeParser::parse(input);
                std::cout << "  ✔ Passed: " << input << ": " << root.toString() << "\n";
                (void)root;
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

    // -----------------------------------------
    // 2) *_infoslice_tests.txt (pipe format)
    //    Line format:  <bencode>|<expected_info_slice>
    //    To assert no info slice: leave RHS empty
    // -----------------------------------------
    for (const auto &entry : fs::directory_iterator(tests_dir)) {

        if (!entry.is_regular_file()) continue;

        const auto fname = entry.path().filename().string();
        if (fname.find("_infoslice_tests.txt") == std::string::npos) continue;

        std::cout << "\nRunning info-slice tests from: " << entry.path().filename() << "\n";
        auto cases = loadLines(entry.path());

        for (auto line : cases) {
            total++;

            // split on first '|'
            std::string benc, expected;
            if (auto pos = line.find('|'); pos != std::string::npos) {
                benc = trim(line.substr(0, pos));
                expected = trim(line.substr(pos + 1));
            } else {
                // fallback: whole line is the bencode; expect empty slice
                benc = trim(line);
                expected.clear();
            }

            try {
                auto res = bencode::BencodeParser::parseWithInfoSlice(benc);
                std::optional<std::string_view> got_sv = res.infoSlice;

                const bool expect_present = !expected.empty();

                if (expect_present) {
                    if (!got_sv) {
                        std::cout << "  ✘ Failed: expected an info slice, but none captured.\n"
                                  << "    bencode: " << benc << "\n";
                    } else if (*got_sv != expected) {
                        std::cout << "  ✘ Failed: info slice mismatch\n"
                                  << "    bencode:  " << benc << "\n"
                                  << "    expected: " << expected << "\n"
                                  << "    got:      " << std::string(*got_sv) << "\n"
                                  << "    expected_hex: " << hexlify(expected) << "\n"
                                  << "    got_hex:      " << hexlify(*got_sv) << "\n";
                    } else {
                        std::cout << "  ✔ Passed: captured info slice\n"
                                  << "    slice: " << expected
                                  << "  (hex " << hexlify(expected) << ")\n";
                        passed++;
                    }
                } else {
                    if (got_sv) {
                        std::cout << "  ✘ Failed: expected no info slice, but captured one\n"
                                  << "    bencode: " << benc << "\n"
                                  << "    got:     " << std::string(*got_sv) << "\n";
                    } else {
                        std::cout << "  ✔ Passed: no info slice captured (as expected)\n";
                        passed++;
                    }
                }
            } catch (const std::exception &ex) {
                std::cout << "  ✘ Failed: exception during parseWithInfoSlice\n"
                          << "    bencode: " << benc << "\n"
                          << "    ex: " << ex.what() << "\n";
            } catch (...) {
                std::cout << "  ✘ Failed: unknown exception during parseWithInfoSlice\n"
                          << "    bencode: " << benc << "\n";
            }
        }
    }

    std::cout << "\nSummary: " << passed << "/" << total << " tests passed.\n";
    return (passed == total) ? 0 : 1;
}
