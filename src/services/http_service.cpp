#include "http_service.h"
#include "task_service.h"
#include <cstring>
#include <plog/Log.h>

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

CURL* HttpService::createCurlHandle() {
    // 每次请求创建新的 curl handle，确保线程安全
    CURL* handle = curl_easy_init();
    if (!handle) return nullptr;

    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, static_cast<long>(timeout_seconds_));
    return handle;
}

bool HttpService::get(const char* url, HttpResponse& response) {
    // 注意：此方法仅用于后台线程，主线程请使用 getAsync
    CURL* handle = createCurlHandle();
    if (!handle) {
        PLOGE << "Failed to create CURL handle";
        return false;
    }

    response = {};
    char full[512]{0};
    if (url[0] == '/') {
        std::snprintf(full, sizeof(full), "%s%s", base_url_.data(), url);
    } else {
        std::snprintf(full, sizeof(full), "%s", url);
    }
    curl_easy_setopt(handle, CURLOPT_URL, full);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response);
    
    CURLcode res = curl_easy_perform(handle);
    
    // 获取HTTP状态码（即使请求失败也可能有状态码）
    if (res == CURLE_OK) {
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response.status_code);
    } else {
        // 请求失败，尝试获取状态码（可能为0）
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response.status_code);
        
        // 根据CURL错误码分类错误类型
        const char* error_msg = curl_easy_strerror(res);
        if (res == CURLE_COULDNT_CONNECT || res == CURLE_COULDNT_RESOLVE_HOST) {
            PLOGW << "Network connection failed: " << error_msg << " (URL: " << full << ")";
        } else if (res == CURLE_OPERATION_TIMEDOUT) {
            PLOGW << "Network request timeout: " << error_msg << " (URL: " << full << ")";
        } else if (res == CURLE_SSL_CONNECT_ERROR) {
            PLOGW << "SSL connection error: " << error_msg << " (URL: " << full << ")";
        } else {
            PLOGW << "Network request failed: " << error_msg << " (CURLcode: " << res << ", URL: " << full << ")";
        }
    }
    
    curl_easy_cleanup(handle);
    
    // 只有HTTP 200才算成功
    bool success = (res == CURLE_OK && response.status_code == 200);
    if (!success && response.status_code > 0) {
        PLOGW << "HTTP request failed with status code: " << response.status_code << " (URL: " << full << ")";
    }
    
    return success;
}

bool HttpService::post(const char* url, const char* json_data, HttpResponse& response) {
    // 注意：此方法仅用于后台线程，主线程请使用 postAsync
    CURL* handle = createCurlHandle();
    if (!handle) {
        PLOGE << "Failed to create CURL handle for POST";
        return false;
    }

    response = {};
    char full[512]{0};
    if (url[0] == '/') {
        std::snprintf(full, sizeof(full), "%s%s", base_url_.data(), url);
    } else {
        std::snprintf(full, sizeof(full), "%s", url);
    }
    curl_easy_setopt(handle, CURLOPT_URL, full);
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(handle);
    
    // 获取HTTP状态码（即使请求失败也可能有状态码）
    if (res == CURLE_OK) {
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response.status_code);
    } else {
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response.status_code);
        const char* error_msg = curl_easy_strerror(res);
        if (res == CURLE_COULDNT_CONNECT || res == CURLE_COULDNT_RESOLVE_HOST) {
            PLOGW << "Network connection failed (POST): " << error_msg << " (URL: " << full << ")";
        } else if (res == CURLE_OPERATION_TIMEDOUT) {
            PLOGW << "Network request timeout (POST): " << error_msg << " (URL: " << full << ")";
        } else {
            PLOGW << "Network request failed (POST): " << error_msg << " (CURLcode: " << res << ", URL: " << full << ")";
        }
    }
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(handle);
    
    // 只有HTTP 200才算成功
    bool success = (res == CURLE_OK && response.status_code == 200);
    if (!success && response.status_code > 0) {
        PLOGW << "HTTP POST failed with status code: " << response.status_code << " (URL: " << full << ")";
    }
    
    return success;
}

void HttpService::getAsync(const std::string& url, std::function<void(bool, HttpResponse)> callback) {
    if (!callback) {
        PLOGW << "HttpService::getAsync: empty callback provided";
        return;
    }

    // 在后台线程执行网络请求
    TaskService::getInstance().runAsync([this, url, callback]() {
        HttpResponse resp;
        bool success = this->get(url.c_str(), resp);
        
        // 完成后回到UI线程执行回调
        TaskService::getInstance().runOnUIThread([success, resp, callback]() {
            callback(success, resp);
        });
    });
}

void HttpService::postAsync(const std::string& url, const std::string& json_data, 
                            std::function<void(bool, HttpResponse)> callback) {
    if (!callback) {
        PLOGW << "HttpService::postAsync: empty callback provided";
        return;
    }

    // 在后台线程执行网络请求
    TaskService::getInstance().runAsync([this, url, json_data, callback]() {
        HttpResponse resp;
        bool success = this->post(url.c_str(), json_data.c_str(), resp);
        
        // 完成后回到UI线程执行回调
        TaskService::getInstance().runOnUIThread([success, resp, callback]() {
            callback(success, resp);
        });
    });
}

}  // namespace ktv::services

