#include "../include/bittorrent/tracker.hpp"


std::string urlEncode(const std::string& data_binary) {

    CURL *curl = curl_easy_init();

    if (!curl) throw std::runtime_error("Curl curl not initialized");

    const char* input_cstr = data_binary.c_str();
    int input_length = static_cast<int>(data_binary.length());

    char *encoded_value = curl_easy_escape(curl, input_cstr, input_length);

    std::string result;
    if (encoded_value) {
        result = encoded_value;
        curl_free(encoded_value);
    }

    curl_easy_cleanup(curl);
    return result;
}