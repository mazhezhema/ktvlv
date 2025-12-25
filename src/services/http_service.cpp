#include "http_service.h"
#include <cstring>

namespace ktv::services {

bool HttpService::initialize(const std::string& base_url, int timeout_seconds) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl_handle_ = curl_easy_init();
    if (!curl_handle_) return false;

    curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl_handle_, CURLOPT_FOLLOWLOCATION, 1L);
    timeout_seconds_ = timeout_seconds;
    curl_easy_setopt(curl_handle_, CURLOPT_TIMEOUT, static_cast<long>(timeout_seconds_));

    std::strncpy(base_url_.data(), base_url.c_str(), base_url_.size() - 1);
    base_url_[base_url_.size() - 1] = '\0';
    return true;
}

void HttpService::cleanup() {
    if (curl_handle_) {
        curl_easy_cleanup(curl_handle_);
        curl_handle_ = nullptr;
    }
    curl_global_cleanup();
}

size_t HttpService::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    HttpResponse* response = static_cast<HttpResponse*>(userp);
    size_t total = size * nmemb;
    if (response->body_len + total < response->body.size()) {
        std::memcpy(response->body.data() + response->body_len, contents, total);
        response->body_len += total;
        response->body[response->body_len] = '\0';
    }
    return total;
}

bool HttpService::get(const char* url, HttpResponse& response) {
    if (!curl_handle_) return false;
    response = {};
    char full[512]{0};
    if (url[0] == '/') {
        std::snprintf(full, sizeof(full), "%s%s", base_url_.data(), url);
    } else {
        std::snprintf(full, sizeof(full), "%s", url);
    }
    curl_easy_setopt(curl_handle_, CURLOPT_URL, full);
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &response);
    CURLcode res = curl_easy_perform(curl_handle_);
    if (res != CURLE_OK) return false;
    curl_easy_getinfo(curl_handle_, CURLINFO_RESPONSE_CODE, &response.status_code);
    return response.status_code == 200;
}

bool HttpService::post(const char* url, const char* json_data, HttpResponse& response) {
    if (!curl_handle_) return false;
    response = {};
    char full[512]{0};
    if (url[0] == '/') {
        std::snprintf(full, sizeof(full), "%s%s", base_url_.data(), url);
    } else {
        std::snprintf(full, sizeof(full), "%s", url);
    }
    curl_easy_setopt(curl_handle_, CURLOPT_URL, full);
    curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &response);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl_handle_, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl_handle_);
    curl_slist_free_all(headers);
    if (res != CURLE_OK) return false;
    curl_easy_getinfo(curl_handle_, CURLINFO_RESPONSE_CODE, &response.status_code);
    return response.status_code == 200;
}

}  // namespace ktv::services

