#pragma once
#include <string>
#include "expected.hpp"


namespace bittorrent::tracker {


struct HttpResponse { int status{0}; std::string body; };


struct IHttpClient 
{
    virtual ~IHttpClient() = default;
    virtual Expected<HttpResponse> get(const std::string& url, int connectTimeoutSec, int transferTimeoutSec, bool followRedirects) = 0;
};


} // namespace bittorrent::tracker