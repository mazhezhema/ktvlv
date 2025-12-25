#ifndef KTVLV_SERVICES_HTTP_SERVICE_H
#define KTVLV_SERVICES_HTTP_SERVICE_H

#include <array>
#include <string>
#include <curl/curl.h>

namespace ktv::services {

struct HttpResponse {
    long status_code{0};
    std::array<char, 8192> body{};
    size_t body_len{0};
};

class HttpService {
public:
    static HttpService& getInstance() {
        static HttpService instance;
        return instance;
    }
    HttpService(const HttpService&) = delete;
    HttpService& operator=(const HttpService&) = delete;

    bool initialize(const std::string& base_url, int timeout_seconds = 10);
    void cleanup();

    bool get(const char* url, HttpResponse& response);
    bool post(const char* url, const char* json_data, HttpResponse& response);

private:
    HttpService() = default;
    ~HttpService() = default;

    CURL* curl_handle_{nullptr};
    std::array<char, 256> base_url_{};
    int timeout_seconds_{10};

    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
};

}  // namespace ktv::services

#endif  // KTVLV_SERVICES_HTTP_SERVICE_H

