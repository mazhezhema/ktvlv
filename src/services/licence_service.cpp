#include "licence_service.h"
#include "http_service.h"
#include "cJSON.h"
#include <plog/Log.h>
#include <cstdio>

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#else
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#endif

namespace ktv::services {

bool LicenceService::initialize() {
    // 占位：实际应加载本地文件
    info_.status = LicenceStatus::NotActivated;
    return true;
}

bool LicenceService::verify(const std::string& licence_code) {
    // 占位：实际应调用服务器验证
    info_.licence_code = licence_code;
    info_.status = LicenceStatus::Trial;
    PLOGI << "Licence verify mock, code=" << licence_code;
    return true;
}

std::string LicenceService::getMacAddress() {
    std::string mac;
    
#ifdef _WIN32
    // Windows: 使用 GetAdaptersInfo
    IP_ADAPTER_INFO adapterInfo[16];
    DWORD dwBufLen = sizeof(adapterInfo);
    DWORD dwStatus = GetAdaptersInfo(adapterInfo, &dwBufLen);
    
    if (dwStatus == ERROR_SUCCESS) {
        PIP_ADAPTER_INFO pAdapterInfo = adapterInfo;
        // 获取第一个非回环适配器的 MAC 地址
        while (pAdapterInfo) {
            if (pAdapterInfo->Type == MIB_IF_TYPE_ETHERNET || 
                pAdapterInfo->Type == IF_TYPE_IEEE80211) {
                char mac_str[18];
                std::snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                             pAdapterInfo->Address[0], pAdapterInfo->Address[1],
                             pAdapterInfo->Address[2], pAdapterInfo->Address[3],
                             pAdapterInfo->Address[4], pAdapterInfo->Address[5]);
                mac = mac_str;
                break;
            }
            pAdapterInfo = pAdapterInfo->Next;
        }
    }
    
    // 如果没找到，使用默认值（用于测试）
    if (mac.empty()) {
        mac = "00:00:00:00:00:01";
    }
#else
    // Linux: 使用 ioctl
    struct ifreq ifr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0) {
        strncpy(ifr.ifr_name, "eth0", IFNAMSIZ - 1);
        if (ioctl(sock, SIOCGIFHWADDR, &ifr) >= 0) {
            unsigned char* addr = (unsigned char*)ifr.ifr_hwaddr.sa_data;
            char mac_str[18];
            std::snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                         addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
            mac = mac_str;
        }
        close(sock);
    }
    
    // 如果没找到，使用默认值
    if (mac.empty()) {
        mac = "00:00:00:00:00:01";
    }
#endif
    
    return mac;
}

std::string LicenceService::getTokenFromLicense(const std::string& license, 
                                                  const std::string& company, 
                                                  const std::string& app_name,
                                                  const std::string& macid) {
    if (license.empty()) {
        PLOGW << "Empty license provided";
        return "";
    }
    
    // 如果没有提供 macid，自动获取 MAC 地址
    std::string mac = macid.empty() ? getMacAddress() : macid;
    
    char url[512];
    if (!company.empty() && !app_name.empty()) {
        // 完整的 URL：包含 license, company, app_name, macid
        std::snprintf(url, sizeof(url), "/karaoke_sdk/vod_token_by_macid?license=%s&company=%s&app_name=%s&macid=%s", 
                     license.c_str(), company.c_str(), app_name.c_str(), mac.c_str());
    } else {
        // 至少包含 license 和 macid
        std::snprintf(url, sizeof(url), "/karaoke_sdk/vod_token_by_macid?license=%s&macid=%s", 
                     license.c_str(), mac.c_str());
    }
    
    printf("DEBUG: Requesting token from: %s\n", url);
    fflush(stdout);
    PLOGD << "Requesting token from license: " << license;
    
    HttpResponse resp;
    bool http_ok = HttpService::getInstance().get(url, resp);
    
    // 无论成功与否，都保存响应以便调试
    printf("DEBUG: HTTP request %s. Status code: %ld, Response length: %zu bytes\n", 
           http_ok ? "succeeded" : "failed", resp.status_code, resp.body_len);
    if (resp.body_len > 0) {
        printf("DEBUG: Response preview (first 500 chars): %s\n", 
               std::string(resp.body.data(), resp.body_len > 500 ? 500 : resp.body_len).c_str());
    }
    fflush(stdout);
    
    // 保存完整响应到文件（无论成功与否）
    FILE* debug_file = fopen("debug_token_response.json", "wb");
    if (debug_file) {
        if (resp.body_len > 0) {
            fwrite(resp.body.data(), 1, resp.body_len, debug_file);
        }
        fclose(debug_file);
        printf("DEBUG: Full token response saved to debug_token_response.json (status: %ld)\n", resp.status_code);
        fflush(stdout);
    }
    
    if (!http_ok) {
        // 网络失败是正常现象（离线、网络不通、服务器异常等）
        // 不抛出异常，返回空token，程序会继续运行（使用离线模式）
        printf("WARNING: Token request failed (this is normal when offline). Status code: %ld\n", resp.status_code);
        if (resp.body_len > 0) {
            printf("WARNING: Response body: %s\n", resp.body.data());
        }
        fflush(stdout);
        PLOGW << "Failed to get token from license: " << license << ", status: " << resp.status_code;
        PLOGW << "This is normal when offline or network is unavailable, application will continue in offline mode";
        return "";
    }
    
    // 解析 JSON 响应
    cJSON* root = cJSON_Parse(resp.body.data());
    if (!root) {
        printf("ERROR: Failed to parse JSON. Response: %s\n", resp.body.data());
        fflush(stdout);
        PLOGW << "Failed to parse token response JSON";
        return "";
    }
    
    // 尝试多种可能的字段名：token, data.token, result.token
    std::string token;
    cJSON* token_item = cJSON_GetObjectItem(root, "token");
    if (token_item) {
        printf("DEBUG: Found 'token' at root level\n");
        fflush(stdout);
    } else {
        cJSON* data = cJSON_GetObjectItem(root, "data");
        if (data && cJSON_IsObject(data)) {
            token_item = cJSON_GetObjectItem(data, "token");
            if (token_item) {
                printf("DEBUG: Found 'token' in data object\n");
                fflush(stdout);
            }
        }
    }
    if (!token_item) {
        cJSON* result = cJSON_GetObjectItem(root, "result");
        if (result && cJSON_IsObject(result)) {
            token_item = cJSON_GetObjectItem(result, "token");
            if (token_item) {
                printf("DEBUG: Found 'token' in result object\n");
                fflush(stdout);
            }
        }
    }
    
    // 如果还没找到，列出所有可用字段
    if (!token_item) {
        printf("WARNING: Token field not found. Available root keys:\n");
        cJSON* child = root->child;
        while (child) {
            const char* type_str = cJSON_IsArray(child) ? "array" : 
                                  (cJSON_IsObject(child) ? "object" : "other");
            printf("  - %s (type: %s)\n", child->string ? child->string : "(null)", type_str);
            child = child->next;
        }
        fflush(stdout);
    }
    
    if (token_item && cJSON_IsString(token_item)) {
        token = token_item->valuestring;
        printf("SUCCESS: Got token (length: %zu)\n", token.length());
        fflush(stdout);
        PLOGI << "Successfully got token from license: " << license;
    } else {
        printf("ERROR: Token not found or not a string. Response structure:\n");
        // 输出 JSON 结构
        char* json_str = cJSON_Print(root);
        if (json_str) {
            printf("%s\n", json_str);
            free(json_str);
        }
        fflush(stdout);
        PLOGW << "Token not found in response. Response preview: " << std::string(resp.body.data(), resp.body_len > 200 ? 200 : resp.body_len);
    }
    
    cJSON_Delete(root);
    return token;
}

bool LicenceService::getRuntimeConfig(const std::string& token,
                                       const std::string& platform,
                                       const std::string& company,
                                       const std::string& app_name,
                                       const std::string& vn) {
    if (token.empty()) {
        PLOGW << "Empty token provided for runtime config";
        return false;
    }
    
    char url[512];
    std::snprintf(url, sizeof(url), 
                  "/karaoke_sdk/vod_conf?platform=%s&token=%s&company=%s&app_name=%s&vn=%s",
                  platform.c_str(), token.c_str(), company.c_str(), app_name.c_str(), vn.c_str());
    
    printf("DEBUG: Requesting runtime config from: %s\n", url);
    fflush(stdout);
    PLOGD << "Requesting runtime config with token";
    
    HttpResponse resp;
    bool http_ok = HttpService::getInstance().get(url, resp);
    
    if (!http_ok) {
        // 运行时配置获取失败是正常现象（离线、网络不通、服务器异常等）
        // 不影响程序运行，返回false表示使用默认配置
        printf("WARNING: Runtime config request failed (network unavailable or offline). Status code: %ld\n", resp.status_code);
        if (resp.body_len > 0) {
            printf("WARNING: Response: %s\n", resp.body.data());
        }
        printf("WARNING: This is normal - application will continue with default config\n");
        fflush(stdout);
        PLOGW << "Failed to get runtime config (network unavailable), status: " << resp.status_code;
        PLOGW << "This is normal when offline or network is unavailable";
        return false;
    }
    
    printf("DEBUG: Runtime config received (length: %zu bytes)\n", resp.body_len);
    if (resp.body_len > 0) {
        printf("DEBUG: Config preview (first 500 chars): %s\n",
               std::string(resp.body.data(), resp.body_len > 500 ? 500 : resp.body_len).c_str());
    }
    fflush(stdout);
    
    // 保存配置到文件以便调试
    FILE* debug_file = fopen("debug_config_response.json", "wb");
    if (debug_file) {
        if (resp.body_len > 0) {
            fwrite(resp.body.data(), 1, resp.body_len, debug_file);
        }
        fclose(debug_file);
        printf("DEBUG: Runtime config saved to debug_config_response.json\n");
        fflush(stdout);
    }
    
    // 解析配置（可选，当前只保存，后续可以根据需要解析特定字段）
    cJSON* root = cJSON_Parse(resp.body.data());
    if (root) {
        // 可以在这里解析特定配置项，如 is_log_upload, features 等
        // 当前只记录成功
        cJSON_Delete(root);
        PLOGI << "Runtime config parsed successfully";
    } else {
        PLOGW << "Failed to parse runtime config JSON";
    }
    
    return true;
}

std::string LicenceService::checkUpdate(const std::string& token,
                                         const std::string& platform,
                                         const std::string& vn,
                                         const std::string& license,
                                         const std::string& company,
                                         const std::string& app_name) {
    if (token.empty() || license.empty()) {
        PLOGW << "Empty token or license for update check";
        return "";
    }
    
    char url[512];
    std::snprintf(url, sizeof(url),
                  "/karaoke_sdk/vod_update?platform=%s&token=%s&vn=%s&license=%s&company=%s&app_name=%s",
                  platform.c_str(), token.c_str(), vn.c_str(), license.c_str(), 
                  company.c_str(), app_name.c_str());
    
    printf("DEBUG: Checking for updates: %s\n", url);
    fflush(stdout);
    PLOGD << "Checking for VOD updates";
    
    HttpResponse resp;
    bool http_ok = HttpService::getInstance().get(url, resp);
    
    if (!http_ok) {
        // 更新检查失败是正常现象（离线、网络不通、服务器异常等）
        // 不影响程序运行，返回空字符串表示无更新
        printf("WARNING: Update check failed (network unavailable or offline). Status code: %ld\n", resp.status_code);
        printf("WARNING: This is normal - application will continue without update check\n");
        fflush(stdout);
        PLOGW << "Update check failed (network unavailable), status: " << resp.status_code;
        PLOGW << "This is normal when offline or network is unavailable";
        return "";
    }
    
    // 响应可能是 URL 字符串或 "null"
    std::string update_url;
    if (resp.body_len > 0) {
        update_url = std::string(resp.body.data(), resp.body_len);
        // 去除可能的换行符和空白
        while (!update_url.empty() && (update_url.back() == '\n' || update_url.back() == '\r' || update_url.back() == ' ')) {
            update_url.pop_back();
        }
        
        if (update_url == "null" || update_url.empty()) {
            printf("DEBUG: No update available\n");
            fflush(stdout);
            return "";
        } else {
            printf("DEBUG: Update available: %s\n", update_url.c_str());
            fflush(stdout);
            PLOGI << "Update URL: " << update_url;
        }
    }
    
    return update_url;
}

}  // namespace ktv::services

