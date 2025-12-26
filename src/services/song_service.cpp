#include "song_service.h"
#include "http_service.h"
#include "cache_service.h"
#include "task_service.h"
#include "cJSON.h"
#include <plog/Log.h>
#include <cstring>
#include <string>

namespace ktv::services {

static void parse_song_array(const char* json_str, std::vector<SongItem>& out) {
    if (!json_str || strlen(json_str) == 0) {
        PLOGW << "parse_song_array: empty JSON string";
        return;
    }
    
    // 调试：输出 JSON 的前 500 个字符
    size_t preview_len = strlen(json_str);
    if (preview_len > 500) preview_len = 500;
    std::string preview(json_str, preview_len);
    PLOGD << "parse_song_array: JSON preview (first " << preview_len << " chars): " << preview;
    
    // 也输出到控制台，确保能看到
    printf("JSON preview (first %zu chars): %s\n", preview_len, preview.c_str());
    fflush(stdout);
    
    cJSON* root = cJSON_Parse(json_str);
    if (!root) {
        PLOGW << "parse_song_array: JSON parse failed";
        return;
    }
    
    // 检查是否是直接数组
    cJSON* array_to_parse = nullptr;
    if (cJSON_IsArray(root)) {
        array_to_parse = root;
        PLOGD << "parse_song_array: JSON is a direct array";
    } else if (cJSON_IsObject(root)) {
        // 首先尝试直接查找数组字段
        const char* direct_keys[] = {"data", "songs", "list", "items", "result", "content"};
        for (const char* key : direct_keys) {
            cJSON* item = cJSON_GetObjectItem(root, key);
            if (item && cJSON_IsArray(item)) {
                array_to_parse = item;
                PLOGD << "parse_song_array: Found array in key: " << key;
                break;
            }
        }
        
        // 如果没找到，尝试嵌套路径：data.mediainfo.list
        if (!array_to_parse) {
            cJSON* data = cJSON_GetObjectItem(root, "data");
            if (data && cJSON_IsObject(data)) {
                cJSON* mediainfo = cJSON_GetObjectItem(data, "mediainfo");
                if (mediainfo && cJSON_IsObject(mediainfo)) {
                    cJSON* list = cJSON_GetObjectItem(mediainfo, "list");
                    if (list && cJSON_IsArray(list)) {
                        array_to_parse = list;
                        PLOGD << "parse_song_array: Found array in data.mediainfo.list";
                    }
                }
            }
        }
        
        // 如果还是没找到，尝试其他常见的嵌套结构
        if (!array_to_parse) {
            cJSON* data = cJSON_GetObjectItem(root, "data");
            if (data && cJSON_IsObject(data)) {
                // 尝试 data.list, data.items, data.result 等
                const char* nested_keys[] = {"list", "items", "result", "content", "songs"};
                for (const char* key : nested_keys) {
                    cJSON* item = cJSON_GetObjectItem(data, key);
                    if (item && cJSON_IsArray(item)) {
                        array_to_parse = item;
                        PLOGD << "parse_song_array: Found array in data." << key;
                        break;
                    }
                }
            }
        }
        
        if (!array_to_parse) {
            PLOGW << "parse_song_array: JSON is an object but no array field found. Available keys:";
            printf("WARNING: JSON is an object but no array field found. Available keys:\n");
            cJSON* child = root->child;
            while (child) {
                const char* type_str = cJSON_IsArray(child) ? "array" : (cJSON_IsObject(child) ? "object" : "other");
                PLOGW << "  - " << child->string << " (type: " << type_str << ")";
                printf("  - %s (type: %s)\n", child->string ? child->string : "(null)", type_str);
                child = child->next;
            }
            fflush(stdout);
        }
    } else {
        PLOGW << "parse_song_array: JSON is neither array nor object";
        cJSON_Delete(root);
        return;
    }
    
    if (!array_to_parse) {
        PLOGW << "parse_song_array: No array found in JSON";
        cJSON_Delete(root);
        return;
    }
    
    int n = cJSON_GetArraySize(array_to_parse);
    PLOGD << "parse_song_array: Found array with " << n << " items";
    printf("parse_song_array: Found array with %d items\n", n);
    fflush(stdout);
    for (int i = 0; i < n; ++i) {
        // 修复：使用 array_to_parse 而不是 root，因为数组可能来自对象字段
        cJSON* item = cJSON_GetArrayItem(array_to_parse, i);
        if (!item) continue;
        SongItem s;
        
        // 支持多种字段名格式：
        // 1. 新格式：songId (数字), songName, singerName
        // 2. 旧格式：song_id, song_name, artist
        cJSON* id_item = cJSON_GetObjectItem(item, "songId");
        if (id_item) {
            // songId 可能是数字或字符串
            if (cJSON_IsNumber(id_item)) {
                char id_str[32];
                std::snprintf(id_str, sizeof(id_str), "%d", (int)cJSON_GetNumberValue(id_item));
                s.id = id_str;
            } else if (cJSON_IsString(id_item)) {
                s.id = id_item->valuestring;
            }
        } else {
            // 尝试旧格式
            cJSON* id = cJSON_GetObjectItem(item, "song_id");
            if (id && cJSON_IsString(id)) s.id = id->valuestring;
        }
        
        // 标题：songName 或 song_name
        cJSON* title_item = cJSON_GetObjectItem(item, "songName");
        if (title_item && cJSON_IsString(title_item)) {
            s.title = title_item->valuestring;
        } else {
            cJSON* title = cJSON_GetObjectItem(item, "song_name");
            if (title && cJSON_IsString(title)) s.title = title->valuestring;
        }
        
        // 艺术家：singerName 或 artist
        cJSON* artist_item = cJSON_GetObjectItem(item, "singerName");
        if (artist_item && cJSON_IsString(artist_item)) {
            s.artist = artist_item->valuestring;
        } else {
            cJSON* artist = cJSON_GetObjectItem(item, "artist");
            if (artist && cJSON_IsString(artist)) s.artist = artist->valuestring;
        }
        
        // 其他字段（保持兼容）
        cJSON* url = cJSON_GetObjectItem(item, "m3u8_url");
        cJSON* cover_url = cJSON_GetObjectItem(item, "cover_url");
        cJSON* artist_image_url = cJSON_GetObjectItem(item, "artist_image_url");
        cJSON* album = cJSON_GetObjectItem(item, "album");
        cJSON* duration = cJSON_GetObjectItem(item, "duration");
        
        if (url && cJSON_IsString(url)) s.m3u8_url = url->valuestring;
        if (cover_url && cJSON_IsString(cover_url)) s.cover_url = cover_url->valuestring;
        if (artist_image_url && cJSON_IsString(artist_image_url)) s.artist_image_url = artist_image_url->valuestring;
        if (album && cJSON_IsString(album)) s.album = album->valuestring;
        if (duration && cJSON_IsNumber(duration)) s.duration = static_cast<int>(duration->valuedouble);
        
        // fallback: if no id, use title as id
        if (s.id.empty() && !s.title.empty()) s.id = s.title;
        if (!s.title.empty()) {
            out.push_back(std::move(s));
            PLOGD << "parse_song_array: Parsed song #" << (out.size()) << ": " << s.title;
        } else {
            PLOGW << "parse_song_array: Skipped item #" << (i+1) << " (no title)";
        }
    }
    cJSON_Delete(root);
    PLOGD << "parse_song_array: Total parsed: " << out.size() << " songs";
}

std::vector<SongItem> SongService::listSongs(int page, int size) {
    std::vector<SongItem> result;
    HttpResponse resp;
    char url[512]{0};
    std::snprintf(url, sizeof(url),
                  "/kcloud/getmusics?token=%s&page=%d&size=%d&company=%s&app_name=%s&platform=%s&vn=%s",
                  token_.c_str(), page, size, net_cfg_.company.c_str(), net_cfg_.app_name.c_str(),
                  net_cfg_.platform.c_str(), net_cfg_.vn.c_str());
    PLOGD << "Calling HttpService::get for: " << url;
    printf("DEBUG: SongService::listSongs - token length: %zu, token empty: %s\n", 
           token_.length(), token_.empty() ? "YES" : "NO");
    if (!token_.empty()) {
        printf("DEBUG: Token preview: %s...\n", token_.substr(0, token_.length() > 20 ? 20 : token_.length()).c_str());
    }
    fflush(stdout);
    bool http_ok = HttpService::getInstance().get(url, resp);
    PLOGD << "HttpService::get returned: " << (http_ok ? "success" : "failed");
    if (!http_ok) {
        // 网络失败是正常现象（离线、网络不通、服务器异常等）
        // 不抛出异常，优雅降级到缓存或mock数据
        PLOGW << "listSongs HTTP failed (status: " << resp.status_code << "), this is normal when offline";
        return result;
    }
    PLOGD << "Parsing response, body length: " << resp.body_len;
    
    // 调试：保存完整 JSON 响应到文件
    if (resp.body_len > 0) {
        FILE* debug_file = fopen("debug_response.json", "wb");
        if (debug_file) {
            fwrite(resp.body.data(), 1, resp.body_len, debug_file);
            fclose(debug_file);
            printf("DEBUG: Full JSON response saved to debug_response.json (%zu bytes)\n", resp.body_len);
            fflush(stdout);
        }
    }
    
    parse_song_array(resp.body.data(), result);
    PLOGD << "Parsed " << result.size() << " songs from response";
    return result;
}

std::vector<SongItem> SongService::search(const std::string& keyword, int page, int size) {
    std::vector<SongItem> result;
    HttpResponse resp;
    char url[512]{0};
    std::snprintf(url, sizeof(url),
                  "/apollo/search/actorsong?token=%s&page=%d&size=%d&key=%s&company=%s&app_name=%s",
                  token_.c_str(), page, size, keyword.c_str(), net_cfg_.company.c_str(),
                  net_cfg_.app_name.c_str());
    bool http_ok = HttpService::getInstance().get(url, resp);
    if (!http_ok) {
        // 网络失败是正常现象（离线、网络不通、服务器异常等）
        // 不抛出异常，优雅降级到缓存或mock数据
        PLOGW << "search HTTP failed (status: " << resp.status_code << "), this is normal when offline";
        return result;
    }
    parse_song_array(resp.body.data(), result);
    return result;
}

bool SongService::addToQueue(const std::string& song_id) {
    HttpResponse resp;
    char url[512]{0};
    std::snprintf(url, sizeof(url),
                  "/karaoke_sdk/t/plist/set?token=%s",
                  token_.c_str());
    char body[256]{0};
    std::snprintf(body, sizeof(body), "{\"song_id\":\"%s\"}", song_id.c_str());
    bool http_ok = HttpService::getInstance().post(url, body, resp);
    if (!http_ok) {
        // 网络失败是正常现象（离线、网络不通、服务器异常等）
        // 在离线模式下，点歌功能仍然可以工作（使用本地队列）
        PLOGW << "addToQueue HTTP failed (status: " << resp.status_code << "), this is normal when offline";
        // 返回false，但不会导致程序崩溃，UI层会处理
        return false;
    }
    return true;
}

std::vector<SongItem> SongService::listSongsOfflineFirst(int page, int size) {
    auto& cache = CacheService::getInstance();
    
    // 生成缓存key
    char cache_key[128]{0};
    std::snprintf(cache_key, sizeof(cache_key), "songs_page_%d_size_%d", page, size);
    
    // 先尝试从缓存加载
    std::vector<SongItem> result = cache.loadSongs(cache_key);
    if (!result.empty()) {
        PLOGI << "Loaded " << result.size() << " songs from cache for page " << page;
    }
    
    // 尝试联网更新（注意：这是同步调用，可能会阻塞最多10秒）
    // 网络不通、离线或异常是正常现象，需要优雅处理
    PLOGD << "Attempting network request for songs (may take up to 10 seconds)...";
    std::vector<SongItem> online_result = listSongs(page, size);
    PLOGD << "Network request completed, result size: " << online_result.size();
    
    if (!online_result.empty()) {
        // 联网成功，更新缓存
        cache.saveSongs(cache_key, online_result);
        PLOGI << "Updated cache with " << online_result.size() << " songs from server";
        return online_result;
    } else {
        // 联网失败是正常现象（网络不通、离线、服务器异常等）
        // 使用缓存数据（如果有），这是离线优先架构的核心
        if (!result.empty()) {
            PLOGI << "Network request failed, using cached data (" << result.size() << " songs)";
        } else {
            PLOGW << "Network request failed and no cache available, returning empty result";
            PLOGW << "This is normal when offline or network is unavailable";
        }
        return result;
    }
}

std::vector<SongItem> SongService::searchOfflineFirst(const std::string& keyword, int page, int size) {
    auto& cache = CacheService::getInstance();
    
    // 生成缓存key（包含关键词）
    char cache_key[256]{0};
    std::snprintf(cache_key, sizeof(cache_key), "search_%s_page_%d_size_%d", 
                  keyword.c_str(), page, size);
    
    // 先尝试从缓存加载
    std::vector<SongItem> result = cache.loadSongs(cache_key);
    if (!result.empty()) {
        PLOGI << "Loaded " << result.size() << " search results from cache for: " << keyword;
    }
    
    // 尝试联网更新（不阻塞，失败时继续使用缓存数据）
    // 网络不通、离线或异常是正常现象，需要优雅处理
    std::vector<SongItem> online_result = search(keyword, page, size);
    if (!online_result.empty()) {
        // 联网成功，更新缓存
        cache.saveSongs(cache_key, online_result);
        PLOGI << "Updated search cache with " << online_result.size() << " results from server";
        return online_result;
    } else {
        // 联网失败是正常现象（网络不通、离线、服务器异常等）
        // 使用缓存数据（如果有），这是离线优先架构的核心
        if (!result.empty()) {
            PLOGI << "Network search failed, using cached results (" << result.size() << " items) for: " << keyword;
        } else {
            PLOGW << "Network search failed and no cache available for: " << keyword;
            PLOGW << "This is normal when offline or network is unavailable";
        }
        return result;
    }
}

void SongService::listSongsOfflineFirstAsync(int page, int size, std::function<void(const std::vector<SongItem>&)> callback) {
    if (!callback) {
        PLOGW << "Empty callback provided to listSongsOfflineFirstAsync";
        return;
    }

    // 在后台线程执行耗时操作
    TaskService::getInstance().runAsync([this, page, size, callback]() {
        // 在后台线程执行：先读缓存，再尝试联网
        std::vector<SongItem> result = listSongsOfflineFirst(page, size);
        
        // 完成后回到UI线程执行回调
        TaskService::getInstance().runOnUIThread([result, callback]() {
            callback(result);
        });
    });
}

void SongService::searchOfflineFirstAsync(const std::string& keyword, int page, int size, std::function<void(const std::vector<SongItem>&)> callback) {
    if (!callback) {
        PLOGW << "Empty callback provided to searchOfflineFirstAsync";
        return;
    }

    // 在后台线程执行耗时操作
    TaskService::getInstance().runAsync([this, keyword, page, size, callback]() {
        // 在后台线程执行：先读缓存，再尝试联网
        std::vector<SongItem> result = searchOfflineFirst(keyword, page, size);
        
        // 完成后回到UI线程执行回调
        TaskService::getInstance().runOnUIThread([result, callback]() {
            callback(result);
        });
    });
}

}  // namespace ktv::services

