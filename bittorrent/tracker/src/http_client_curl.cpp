#include <curl/curl.h>
#include <string>
#include <memory>
#include "../include/http_client.hpp"

namespace bittorrent::tracker {

    namespace {

        // libcurl write callback
        size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
            size_t realSize = size * nmemb;
            auto* buffer = static_cast<std::string*>(userp);
            buffer->append(static_cast<char*>(contents), realSize);
            return realSize;
        }

    } // anonymous namespace


    class HttpClientCurl : public IHttpClient 
    {
    public:
        HttpClientCurl() {
            curl_global_init(CURL_GLOBAL_DEFAULT);
        }

        ~HttpClientCurl() override {
            curl_global_cleanup();
        }

        Expected<HttpResponse> get(const std::string& url,
                                int connectTimeout,
                                int totalTimeout,
                                bool followRedirects) override 
        {
            CURL* curl = curl_easy_init();
            
            if (!curl) {
                return Expected<HttpResponse>::failure("curl init failed");
            }

            std::string body;
            char errorBuffer[CURL_ERROR_SIZE] = {0};

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
            curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connectTimeout);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, totalTimeout);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, followRedirects ? 1L : 0L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "mytorrent/0.1");

            CURLcode res = curl_easy_perform(curl);
            long statusCode = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
            curl_easy_cleanup(curl);

            if (res != CURLE_OK) {
                return Expected<HttpResponse>::failure(
                    std::string("curl error: ") +
                    (errorBuffer[0] ? errorBuffer : curl_easy_strerror(res))
                );
            }

            if (statusCode >= 400) {
                return Expected<HttpResponse>::failure(
                    "HTTP status " + std::to_string(statusCode)
                );
            }

            return Expected<HttpResponse>::success(
                HttpResponse{static_cast<int>(statusCode), std::move(body)}
            );
        }
    };


    // Factory helper — link this TU and call for production
    std::shared_ptr<IHttpClient> makeCurlClient() {
        return std::make_shared<HttpClientCurl>();
    }

} // namespace bittorrent::tracker
