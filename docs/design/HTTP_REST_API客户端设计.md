# HTTP REST API客户端设计

> **文档版本**：v1.0  
> **最后更新**：2025-12-30  
> **状态**：⚠️ **参考文档**（服务器API接口列表）  
> **相关文档**：详见 [项目架构设计总览.md](./项目架构设计总览.md)  
> **架构设计**：详见 [C++架构设计-预分配内存版本.md](./C++架构设计-预分配内存版本.md)  
> **开源库选型**：详见 [开源库选型指南.md](./开源库选型指南.md)

## ⚠️ 重要说明

**本文档主要内容**：
- ✅ **服务器端API接口列表**：完整的REST API接口映射（仍有效）
- ⚠️ **HTTP客户端实现设计**：已更新，请参考新的实现指南

**实际实现请参考**：
- ⭐⭐⭐ **[NetworkService与libcurl实现指南（MVP可落地版）.md](../guides/NetworkService与libcurl实现指南（MVP可落地版）.md)** - NetworkService异步Event驱动实现（**推荐**）
- ⭐⭐⭐ **[服务层API设计文档.md](../服务层API设计文档.md)** - NetworkService API接口定义

**服务器端协议已确定**：基于HTTP的REST API，使用JSON格式。客户端使用NetworkService（异步Event驱动）实现HTTP客户端调用。

### 核心原则（实现时参考新文档）

- ✅ **使用libcurl库**：成熟稳定的HTTP客户端库
- ✅ **NetworkService（Singleton）**：libcurl只在NetworkService中使用
- ✅ **异步Event驱动**：网络请求结果通过EventQueue返回
- ✅ **cJSON解析**：使用cJSON解析JSON响应
- ✅ **低代码实现**：通过库简化实现，减少代码量

---

## 一、REST/WS 接口设计（对齐服务器实现）

### 1.1 API 基础信息

- **Base URL**：`https://mc.ktv.com.cn`（从配置或编译常量读取）
- **公共查询参数**：`company`（渠道号）、`app_name`（区分设备）、`platform`（android 版本）、`vn`（版本号），token/license 视接口携带
- **协议**：HTTP(S) GET/POST/DELETE + JSON；WebSocket `wss://mc.ktv.com.cn/ws/{license}`
- **MVP 取舍**：WebSocket 保留；日志上传 `/karaoke_sdk/log/upload` **暂缓**（非 MVP）

### 1.2 已有服务器接口映射

**认证 / Licence / Token / 配置**
- `GET /karaoke_v3/lic?company&app_name`
- `GET /karaoke_sdk/vod_token_by_macid?license`
- `GET /karaoke_sdk/vod_token`
- `GET /karaoke_sdk/vod_conf?platform&token&company&app_name&vn`
- `GET /karaoke_sdk/vod_update?platform&token&vn&license&company&app_name`
- `GET /karaoke_v3/mac_key/check?key&macid&company&app_name`
- `GET /karaoke_yiban/token?yiban_status&custId&macid&company&app_name`
- `GET /karaoke_sdk/t/yinjixie?license&company&app_name&macid&yinjixie&expireData`

**播放 / 点歌 / 点赞**
- `POST /karaoke_sdk/t/plist/set?token=...`（点歌/播放队列）
- `POST /karaoke_sdk/t/like/set?option=0|1|2&token=...`（点赞/取消）

**歌曲 / 歌手 / 模块**
- `GET /kcloud/getmusics?token&page&size&word&language&company&app_name`
- `GET /apollo/search/actorsong?token&page&size&key&unionid&actorid&company&app_name`
- `GET /apollo/module/songlist?token&page&size&id&unionid&company&app_name`
- `GET /vodc/song/list?token&p&tp&size&type&openid&unionid&company&app_name`
- `GET /vodc/usersong/analyse?token&company&app_name&song_ids`
- `GET /kcloud/getsingerlist_new?token&page&size&word&tp&company&app_name`
- `GET /apollo/module/modulelist?id=2`
- `GET /vodc/song/topics`

**下载 / 其它**
- `GET /MusicService.aspx?op=getmusicinfobynos&depot=2&nos=...`
- `DELETE /pub?id=...`

**第三方 / 支付 / 分享**
- `GET https://qnsign.ktvsky.com/qn/sign/v2?file_name=...&bucket=common-web`
- `GET https://mc.ktv.com.cn/yjx/pay?license&company&app_name&macid`
- `GET https://mcfd.ktv.com.cn/mobile_music/share?score=...`

**WebSocket（MVP 保留）**
- `wss://mc.ktv.com.cn/ws/{license}`

> 日志上传 `/karaoke_sdk/log/upload`：**非 MVP，暂不实现，后续按需恢复**。

---

## 二、HTTP客户端服务设计

### 2.1 HTTP客户端服务（HttpService）

```cpp
#ifndef HTTP_SERVICE_H
#define HTTP_SERVICE_H

#include "cJSON.h"  // JSON解析
#include <curl/curl.h>  // libcurl
#include <array>
#include <cstdint>

namespace ktv {

// HTTP响应结果
struct HttpResponse {
    int status_code;  // HTTP状态码（200, 404等）
    std::array<char, 8192> body;  // 响应体（固定大小缓冲区）
    size_t body_len;  // 实际响应体长度
    
    HttpResponse() : status_code(0), body_len(0) {
        body.fill(0);
    }
};

/**
 * HTTP客户端服务（单例，预分配版本，无锁设计）
 * 使用libcurl实现HTTP REST API调用
 * 单线程设计，无需锁保护
 */
class HttpService {
public:
    static HttpService& getInstance() {
        static HttpService instance;
        return instance;
    }
    
    HttpService(const HttpService&) = delete;
    HttpService& operator=(const HttpService&) = delete;
    
    // 初始化（单线程模式）
    bool initialize();
    
    // 清理
    void cleanup();
    
    // GET请求
    bool get(const char* url, HttpResponse& response);
    
    // POST请求
    bool post(const char* url, const char* json_data, HttpResponse& response);
    
    // 设置服务器地址
    void setBaseUrl(const char* base_url);
    
    // 设置超时时间（秒）
    void setTimeout(int timeout_seconds);
    
private:
    HttpService() = default;
    ~HttpService() = default;
    
    // libcurl句柄（单例，预分配）
    CURL* curl_handle_;
    
    // 服务器基础URL（固定大小缓冲区）
    static constexpr size_t MAX_URL_LEN = 256;
    std::array<char, MAX_URL_LEN> base_url_;
    
    // 超时时间（秒）
    int timeout_seconds_;
    
    // 写回调函数（用于接收响应数据）
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
    
    // 禁止动态分配
    void* operator new(size_t) = delete;
    void* operator new[](size_t) = delete;
    void operator delete(void*) = delete;
    void operator delete[](void*) = delete;
};

} // namespace ktv

#endif // HTTP_SERVICE_H
```

### 2.2 HTTP客户端服务实现

```cpp
#include "http_service.h"
#include <cstring>
#include <cstdio>

namespace ktv {

bool HttpService::initialize() {
    // 初始化libcurl（单线程模式）
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    curl_handle_ = curl_easy_init();
    if (!curl_handle_) {
        return false;
    }
    
    // 配置libcurl（单线程模式，无锁）
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl_handle_, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_handle_, CURLOPT_TIMEOUT, 10L);  // 默认10秒超时
    
    // 设置默认服务器地址
    strncpy(base_url_.data(), "http://api.ktv.example.com", MAX_URL_LEN - 1);
    base_url_[MAX_URL_LEN - 1] = '\0';
    
    timeout_seconds_ = 10;
    
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
    size_t total_size = size * nmemb;
    
    // 检查缓冲区是否足够
    if (response->body_len + total_size < response->body.size()) {
        memcpy(response->body.data() + response->body_len, contents, total_size);
        response->body_len += total_size;
        response->body[response->body_len] = '\0';
    }
    
    return total_size;
}

bool HttpService::get(const char* url, HttpResponse& response) {
    // 单线程设计，无需锁保护
    
    response.status_code = 0;
    response.body_len = 0;
    response.body.fill(0);
    
    // 构建完整URL
    char full_url[512] = {0};
    if (url[0] == '/') {
        // 相对路径，拼接base_url
        snprintf(full_url, sizeof(full_url), "%s%s", base_url_.data(), url);
    } else {
        // 绝对路径
        strncpy(full_url, url, sizeof(full_url) - 1);
    }
    
    // 设置URL和响应缓冲区
    curl_easy_setopt(curl_handle_, CURLOPT_URL, full_url);
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &response);
    
    // 执行请求
    CURLcode res = curl_easy_perform(curl_handle_);
    if (res != CURLE_OK) {
        return false;
    }
    
    // 获取HTTP状态码
    curl_easy_getinfo(curl_handle_, CURLINFO_RESPONSE_CODE, &response.status_code);
    
    return (response.status_code == 200);
}

bool HttpService::post(const char* url, const char* json_data, HttpResponse& response) {
    // 单线程设计，无需锁保护
    
    response.status_code = 0;
    response.body_len = 0;
    response.body.fill(0);
    
    // 构建完整URL
    char full_url[512] = {0};
    if (url[0] == '/') {
        snprintf(full_url, sizeof(full_url), "%s%s", base_url_.data(), url);
    } else {
        strncpy(full_url, url, sizeof(full_url) - 1);
    }
    
    // 设置URL、POST数据和响应缓冲区
    curl_easy_setopt(curl_handle_, CURLOPT_URL, full_url);
    curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &response);
    
    // 设置Content-Type
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl_handle_, CURLOPT_HTTPHEADER, headers);
    
    // 执行请求
    CURLcode res = curl_easy_perform(curl_handle_);
    
    // 清理headers
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        return false;
    }
    
    // 获取HTTP状态码
    curl_easy_getinfo(curl_handle_, CURLINFO_RESPONSE_CODE, &response.status_code);
    
    return (response.status_code == 200);
}

void HttpService::setBaseUrl(const char* base_url) {
    strncpy(base_url_.data(), base_url, MAX_URL_LEN - 1);
    base_url_[MAX_URL_LEN - 1] = '\0';
}

void HttpService::setTimeout(int timeout_seconds) {
    timeout_seconds_ = timeout_seconds;
    curl_easy_setopt(curl_handle_, CURLOPT_TIMEOUT, static_cast<long>(timeout_seconds_));
}

} // namespace ktv
```

---

## 三、Licence验证接口（客户端自有 /api/licence/verify 保留）

### 3.1 验证Licence API

**接口定义：**
```
POST /api/licence/verify
Content-Type: application/json

请求体：
{
  "licence": "XXXX-XXXX-XXXX-XXXX",
  "mac_address": "AA:BB:CC:DD:EE:FF"
}

响应（成功）：
{
  "valid": true,
  "duration": 12,  // 有效期（月数）：12, 24, 36等
  "start_time": 1234567890,  // 开始时间（Unix时间戳）
  "message": "验证成功"
}

响应（失败）：
{
  "valid": false,
  "message": "Licence无效或已被使用"
}
```

**使用示例：**
```cpp
// 验证licence
bool verifyLicence(const char* licence_code) {
    // 获取MAC地址
    char mac[18] = {0};
    LicenceService::getMacAddress(mac, sizeof(mac));
    
    // 构建请求JSON
    cJSON* request = cJSON_CreateObject();
    cJSON_AddStringToObject(request, "licence", licence_code);
    cJSON_AddStringToObject(request, "mac_address", mac);
    
    char* json_string = cJSON_Print(request);
    
    // 发送POST请求
    HttpResponse response;
    if (!HttpService::getInstance().post("/api/licence/verify", 
                                         json_string, 
                                         response)) {
        cJSON_Delete(request);
        free(json_string);
        return false;
    }
    
    cJSON_Delete(request);
    free(json_string);
    
    // 解析响应
    cJSON* json = cJSON_Parse(response.body.data());
    if (!json) {
        return false;
    }
    
    cJSON* valid = cJSON_GetObjectItem(json, "valid");
    bool result = cJSON_IsTrue(valid);
    
    if (result) {
        // 获取有效期和开始时间
        cJSON* duration = cJSON_GetObjectItem(json, "duration");
        cJSON* start_time = cJSON_GetObjectItem(json, "start_time");
        // ... 保存到LicenceService
    }
    
    cJSON_Delete(json);
    return result;
}
```

**详见**：[Licence许可证管理系统设计.md](./Licence许可证管理系统设计.md)

---

## 四、歌曲数据服务（SongService）

> 说明：保持现有 SongService 结构不变，但将 URL 构造替换为真实服务器接口：
> - 列表：`/kcloud/getmusics`
> - 搜索：`/apollo/search/actorsong`（type=actor/song 由 key/actorid 组合决定）
> - 模块歌单：`/apollo/module/songlist`
> - 榜单/专题：可用 `/vodc/song/list`（tp/type）或 `/vodc/song/topics`
> - 点歌：仍用 POST `/karaoke_sdk/t/plist/set?token=...`
> - 分类：可复用 `/kcloud/getsingerlist_new` 的 tp 字段或在客户端内置

```cpp
#ifndef SONG_SERVICE_H
#define SONG_SERVICE_H

#include "http_service.h"
#include "models/song_item.h"
#include <array>

namespace ktv {

/**
 * 歌曲数据服务（单例，预分配版本，无锁设计）
 * 通过HTTP REST API获取歌曲数据
 * 单线程设计，无需锁保护
 */
class SongService {
public:
    static SongService& getInstance() {
        static SongService instance;
        return instance;
    }
    
    SongService(const SongService&) = delete;
    SongService& operator=(const SongService&) = delete;
    
    // 获取歌曲列表
    size_t getSongList(SongItem* out_songs, size_t max_count,
                       int page = 1, int size = 20, int category_id = 0);
    
    // 搜索歌曲
    size_t searchSongs(SongItem* out_songs, size_t max_count,
                       const char* keyword, const char* type = "song");
    
    // 获取排行榜
    size_t getRankings(SongItem* out_songs, size_t max_count,
                       const char* type = "hot", int limit = 50);
    
    // 获取分类列表
    size_t getCategories(int* out_ids, char (*out_names)[64], size_t max_count);
    
    // 点歌（添加到播放队列）
    bool addToQueue(const char* song_id);
    
private:
    SongService() = default;
    ~SongService() = default;
    
    // 解析歌曲列表JSON
    size_t parseSongListJson(const char* json_str, SongItem* out_songs, size_t max_count);
    
    // 禁止动态分配
    void* operator new(size_t) = delete;
    void* operator new[](size_t) = delete;
    void operator delete(void*) = delete;
    void operator delete[](void*) = delete;
};

} // namespace ktv

#endif // SONG_SERVICE_H
```

### 3.2 歌曲数据服务实现（示例：真实接口）

```cpp
#include "song_service.h"
#include "cJSON.h"
#include <cstdio>

namespace ktv {

size_t SongService::getSongList(SongItem* out_songs, size_t max_count,
                                 int page, int size, int category_id) {
    // 构建API URL → 对应 /kcloud/getmusics
    char url[512] = {0};
    snprintf(url, sizeof(url),
             "/kcloud/getmusics?token=%s&page=%d&size=%d&language=%s&company=%s&app_name=%s",
             g_token, page, size, g_language, g_company, g_app_name);
    
    // 发送HTTP GET请求
    HttpResponse response;
    if (!HttpService::getInstance().get(url, response)) {
        return 0;
    }
    
    // 解析JSON响应
    return parseSongListJson(response.body.data(), out_songs, max_count);
}

size_t SongService::searchSongs(SongItem* out_songs, size_t max_count,
                                 const char* keyword, const char* type) {
    // 构建API URL → 对应 /apollo/search/actorsong
    char url[512] = {0};
    snprintf(url, sizeof(url),
             "/apollo/search/actorsong?token=%s&page=%d&size=%d&key=%s&company=%s&app_name=%s",
             g_token, 1, 20, keyword, g_company, g_app_name);
    
    // 发送HTTP GET请求
    HttpResponse response;
    if (!HttpService::getInstance().get(url, response)) {
        return 0;
    }
    
    // 解析JSON响应
    return parseSongListJson(response.body.data(), out_songs, max_count);
}

size_t SongService::getRankings(SongItem* out_songs, size_t max_count,
                                  const char* type, int limit) {
    // 构建API URL → 复用 /vodc/song/list 的 tp/type 作为榜单参数
    char url[512] = {0};
    snprintf(url, sizeof(url),
             "/vodc/song/list?token=%s&p=1&tp=%s&size=%d&type=%s&company=%s&app_name=%s",
             g_token, type, limit, type, g_company, g_app_name);
    
    // 发送HTTP GET请求
    HttpResponse response;
    if (!HttpService::getInstance().get(url, response)) {
        return 0;
    }
    
    // 解析JSON响应
    return parseSongListJson(response.body.data(), out_songs, max_count);
}

size_t SongService::getCategories(int* out_ids, char (*out_names)[64], size_t max_count) {
    // （若无直接分类接口，可复用 kcloud/getsingerlist_new 的 tp 字段，或使用本地内置分类）
    const char* url = "/vodc/song/topics";  // 示例：获取专题/分类
    
    // 发送HTTP GET请求
    HttpResponse response;
    if (!HttpService::getInstance().get(url, response)) {
        return 0;
    }
    
    // 解析JSON响应（使用cJSON）
    cJSON* json = cJSON_ParseWithLength(response.body.data(), response.body_len);
    if (!json || !cJSON_IsArray(json)) {
        if (json) cJSON_Delete(json);
        return 0;
    }
    
    size_t count = 0;
    int array_size = cJSON_GetArraySize(json);
    
    for (int i = 0; i < array_size && count < max_count; ++i) {
        cJSON* item = cJSON_GetArrayItem(json, i);
        cJSON* id = cJSON_GetObjectItem(item, "category_id");
        cJSON* name = cJSON_GetObjectItem(item, "category_name");
        
        if (id && name) {
            out_ids[count] = static_cast<int>(cJSON_GetNumberValue(id));
            const char* name_str = cJSON_GetStringValue(name);
            strncpy(out_names[count], name_str, 63);
            out_names[count][63] = '\0';
            count++;
        }
    }
    
    cJSON_Delete(json);
    return count;
}

bool SongService::addToQueue(const char* song_id) {
    // 直接调用服务器点歌接口：POST /karaoke_sdk/t/plist/set?token=...
    char url[512] = {0};
    snprintf(url, sizeof(url), "/karaoke_sdk/t/plist/set?token=%s", g_token);
    
    // 简单表单或JSON均可，这里用 JSON
    char json_data[256] = {0};
    snprintf(json_data, sizeof(json_data), "{\"song_id\":\"%s\"}", song_id);
    
    HttpResponse response;
    if (!HttpService::getInstance().post(url, json_data, response)) {
        return false;
    }
    
    // 解析响应（可选）
    cJSON* json = cJSON_ParseWithLength(response.body.data(), response.body_len);
    if (json) {
        cJSON* success = cJSON_GetObjectItem(json, "success");
        bool result = (success && cJSON_IsTrue(success));
        cJSON_Delete(json);
        return result;
    }
    
    return (response.status_code == 200);
}

size_t SongService::parseSongListJson(const char* json_str, SongItem* out_songs, size_t max_count) {
    // 使用cJSON解析JSON数组
    cJSON* json = cJSON_ParseWithLength(json_str, strlen(json_str));
    if (!json || !cJSON_IsArray(json)) {
        if (json) cJSON_Delete(json);
        return 0;
    }
    
    size_t count = 0;
    int array_size = cJSON_GetArraySize(json);
    
    for (int i = 0; i < array_size && count < max_count; ++i) {
        cJSON* item = cJSON_GetArrayItem(json, i);
        SongItem& song = out_songs[count];
        
        cJSON* song_id = cJSON_GetObjectItem(item, "song_id");
        cJSON* song_name = cJSON_GetObjectItem(item, "song_name");
        cJSON* artist = cJSON_GetObjectItem(item, "artist");
        cJSON* m3u8_url = cJSON_GetObjectItem(item, "m3u8_url");
        cJSON* is_hd = cJSON_GetObjectItem(item, "is_hd");
        cJSON* duration = cJSON_GetObjectItem(item, "duration");
        
        if (song_id) song.setId(cJSON_GetStringValue(song_id));
        if (song_name) song.setName(cJSON_GetStringValue(song_name));
        if (artist) song.setArtist(cJSON_GetStringValue(artist));
        if (m3u8_url) song.setUrl(cJSON_GetStringValue(m3u8_url));
        if (is_hd) song.is_hd = cJSON_IsTrue(is_hd);
        if (duration) song.duration = static_cast<int32_t>(cJSON_GetNumberValue(duration));
        
        count++;
    }
    
    cJSON_Delete(json);
    return count;
}

} // namespace ktv
```

---

## 五、使用示例

### 4.1 初始化HTTP服务

```cpp
#include "http_service.h"
#include "song_service.h"

int main() {
    // 初始化HTTP服务
    HttpService::getInstance().initialize();
    
    // 设置服务器地址（从配置文件读取）
    HttpService::getInstance().setBaseUrl("http://api.ktv.example.com");
    HttpService::getInstance().setTimeout(10);
    
    // ... 其他初始化
    
    return 0;
}
```

### 4.2 获取歌曲列表

```cpp
// 获取歌曲列表（低代码实现）
void HomeTab::loadData() {
    std::array<SongItem, 100> songs;
    size_t count = SongService::getInstance().getSongList(
        songs.data(), songs.size(), 1, 20, 0  // 第1页，每页20条
    );
    
    // 更新UI
    for (size_t i = 0; i < count; ++i) {
        // 添加到列表
    }
}
```

### 4.3 搜索歌曲

```cpp
// 搜索歌曲（低代码实现）
void SearchPage::onSearch(const char* keyword) {
    std::array<SongItem, 100> songs;
    size_t count = SongService::getInstance().searchSongs(
        songs.data(), songs.size(), keyword, "song"
    );
    
    // 显示搜索结果
}
```

### 4.4 点歌

```cpp
// 点歌（低代码实现）
void onSongItemClicked(const SongItem& song) {
    // 添加到播放队列（通过REST API）
    if (SongService::getInstance().addToQueue(song.getId())) {
        // 成功，开始播放
        // playerService->play(song);
    } else {
        // 失败，显示错误
    }
}
```

---

## 六、libcurl集成

### 5.1 编译libcurl

**方式1：使用系统包管理器（推荐）**
```bash
# Tina Linux中，libcurl可能已包含在SDK中
# 或使用opkg安装
opkg install libcurl
```

**方式2：交叉编译静态库**
```bash
# 下载libcurl源码
wget https://curl.se/download/curl-8.x.x.tar.gz
tar -xzf curl-8.x.x.tar.gz
cd curl-8.x.x

# 配置（单线程模式，最小化功能）
./configure --host=arm-linux \
            --prefix=/usr/local \
            --disable-threaded-resolver \
            --disable-ldap \
            --disable-ldaps \
            --without-ssl \
            --enable-static \
            --disable-shared

# 编译
make
make install
```

### 5.2 链接libcurl

```makefile
# Makefile
LIBS = -lcurl -lpthread
CFLAGS = -I/usr/local/include
LDFLAGS = -L/usr/local/lib
```

---

## 七、单线程设计说明

### 6.1 libcurl单线程模式

**libcurl支持单线程模式：**
- 默认情况下，libcurl是线程安全的
- 但可以配置为单线程模式（无锁）
- 使用`CURL_GLOBAL_DEFAULT`初始化（单线程）

### 6.2 预分配缓冲区

**使用固定大小缓冲区接收响应：**
```cpp
struct HttpResponse {
    std::array<char, 8192> body;  // 预分配8KB缓冲区
    size_t body_len;
};
```

**限制：**
- 响应大小不能超过8KB（可根据需要调整）
- 如果响应过大，需要分页请求

---

## 七、错误处理

### 7.1 HTTP错误处理

```cpp
bool HttpService::get(const char* url, HttpResponse& response) {
    // ... 执行请求
    
    if (response.status_code == 200) {
        return true;  // 成功
    } else if (response.status_code == 404) {
        // 资源不存在
        return false;
    } else if (response.status_code >= 500) {
        // 服务器错误
        return false;
    }
    
    return false;
}
```

### 7.2 网络错误处理

```cpp
// 网络错误处理
CURLcode res = curl_easy_perform(curl_handle_);
if (res != CURLE_OK) {
    const char* error = curl_easy_strerror(res);
    LOG_ERROR("HTTP request failed: %s", error);
    return false;
}
```

---

## 八、总结

### 8.1 核心要点

- ✅ **使用libcurl**：成熟稳定的HTTP客户端库
- ✅ **单线程设计**：libcurl支持单线程模式，无锁
- ✅ **预分配缓冲区**：使用固定大小缓冲区接收响应
- ✅ **cJSON解析**：使用cJSON解析JSON响应
- ✅ **低代码实现**：通过库简化实现，减少代码量

### 8.2 API调用流程

```
UI层（页面）
  ↓
SongService（数据服务）
  ↓
HttpService（HTTP客户端，libcurl）
  ↓
HTTP REST API（服务器）
  ↓
JSON响应
  ↓
cJSON解析
  ↓
数据模型（SongItem等）
  ↓
UI更新
```

### 8.3 优势

- ✅ **低代码**：使用现成库，减少代码量
- ✅ **成熟稳定**：libcurl和cJSON都是成熟库
- ✅ **单线程友好**：支持单线程模式，无锁
- ✅ **预分配内存**：使用固定大小缓冲区

---

**总结**：服务器端使用HTTP REST API + JSON，客户端使用libcurl + cJSON实现，单线程设计，预分配内存，低代码实现。

