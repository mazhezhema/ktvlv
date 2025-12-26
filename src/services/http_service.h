#ifndef KTVLV_SERVICES_HTTP_SERVICE_H
#define KTVLV_SERVICES_HTTP_SERVICE_H

#include <array>
#include <string>
#include <functional>
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

    // 同步方法（已废弃，仅用于内部或后台线程）
    bool get(const char* url, HttpResponse& response);
    bool post(const char* url, const char* json_data, HttpResponse& response);

    // 异步方法（推荐使用，UI线程安全）
    void getAsync(const std::string& url, std::function<void(bool, HttpResponse)> callback);
    void postAsync(const std::string& url, const std::string& json_data, 
                   std::function<void(bool, HttpResponse)> callback);

private:
    HttpService() = default;
    ~HttpService() = default;

    CURL* curl_handle_{nullptr};
    std::array<char, 256> base_url_{};
    int timeout_seconds_{10};

    // 创建新的 curl handle（线程安全，每次请求使用独立的 handle）
    CURL* createCurlHandle();

    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
};

}  // namespace ktv::services

#endif  // KTVLV_SERVICES_HTTP_SERVICE_H

