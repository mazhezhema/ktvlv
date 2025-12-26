#include "cache_service.h"
#include "cJSON.h"
#include <plog/Log.h>
#include <fstream>
#include <filesystem>

namespace ktv::services {

bool CacheService::initialize(const std::string& cache_dir) {
    cache_dir_ = cache_dir;
    
    // 创建缓存目录（如果不存在）
    try {
        if (!std::filesystem::exists(cache_dir_)) {
            std::filesystem::create_directories(cache_dir_);
            PLOGI << "Created cache directory: " << cache_dir_;
        }
        initialized_ = true;
        return true;
    } catch (const std::exception& e) {
        PLOGE << "Failed to initialize cache directory: " << e.what();
        return false;
    }
}

std::string CacheService::getCachePath(const std::string& key) const {
    // 将key转换为安全的文件名（替换特殊字符）
    std::string safe_key = key;
    for (char& c : safe_key) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    return cache_dir_ + "/" + safe_key + ".json";
}

bool CacheService::saveSongs(const std::string& key, const std::vector<SongItem>& songs) {
    if (!initialized_) {
        PLOGW << "CacheService not initialized";
        return false;
    }

    // 创建JSON数组
    cJSON* root = cJSON_CreateArray();
    if (!root) {
        PLOGE << "Failed to create JSON array";
        return false;
    }

    for (const auto& song : songs) {
        cJSON* item = cJSON_CreateObject();
        if (!item) continue;

        cJSON_AddStringToObject(item, "id", song.id.c_str());
        cJSON_AddStringToObject(item, "title", song.title.c_str());
        cJSON_AddStringToObject(item, "artist", song.artist.c_str());
        cJSON_AddStringToObject(item, "m3u8_url", song.m3u8_url.c_str());
        cJSON_AddStringToObject(item, "cover_url", song.cover_url.c_str());
        cJSON_AddStringToObject(item, "artist_image_url", song.artist_image_url.c_str());
        cJSON_AddStringToObject(item, "album", song.album.c_str());
        cJSON_AddNumberToObject(item, "duration", song.duration);

        cJSON_AddItemToArray(root, item);
    }

    // 转换为字符串
    char* json_str = cJSON_Print(root);
    if (!json_str) {
        cJSON_Delete(root);
        PLOGE << "Failed to print JSON";
        return false;
    }

    // 写入文件
    std::string path = getCachePath(key);
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        free(json_str);
        cJSON_Delete(root);
        PLOGE << "Failed to open cache file for writing: " << path;
        return false;
    }

    file << json_str;
    file.close();
    free(json_str);
    cJSON_Delete(root);

    PLOGI << "Saved " << songs.size() << " songs to cache: " << key;
    return true;
}

std::vector<SongItem> CacheService::loadSongs(const std::string& key) {
    std::vector<SongItem> result;
    
    if (!initialized_) {
        PLOGW << "CacheService not initialized";
        return result;
    }

    std::string path = getCachePath(key);
    if (!std::filesystem::exists(path)) {
        PLOGD << "Cache file not found: " << path;
        return result;
    }

    // 读取文件
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        PLOGW << "Failed to open cache file: " << path;
        return result;
    }

    std::string json_str((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    if (json_str.empty()) {
        PLOGW << "Cache file is empty: " << path;
        return result;
    }

    // 解析JSON
    cJSON* root = cJSON_Parse(json_str.c_str());
    if (!root || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        PLOGW << "Failed to parse cache JSON: " << path;
        return result;
    }

    int n = cJSON_GetArraySize(root);
    for (int i = 0; i < n; ++i) {
        cJSON* item = cJSON_GetArrayItem(root, i);
        if (!item) continue;

        SongItem song;
        cJSON* id = cJSON_GetObjectItem(item, "id");
        cJSON* title = cJSON_GetObjectItem(item, "title");
        cJSON* artist = cJSON_GetObjectItem(item, "artist");
        cJSON* m3u8_url = cJSON_GetObjectItem(item, "m3u8_url");
        cJSON* cover_url = cJSON_GetObjectItem(item, "cover_url");
        cJSON* artist_image_url = cJSON_GetObjectItem(item, "artist_image_url");
        cJSON* album = cJSON_GetObjectItem(item, "album");
        cJSON* duration = cJSON_GetObjectItem(item, "duration");

        if (id && cJSON_IsString(id)) song.id = id->valuestring;
        if (title && cJSON_IsString(title)) song.title = title->valuestring;
        if (artist && cJSON_IsString(artist)) song.artist = artist->valuestring;
        if (m3u8_url && cJSON_IsString(m3u8_url)) song.m3u8_url = m3u8_url->valuestring;
        if (cover_url && cJSON_IsString(cover_url)) song.cover_url = cover_url->valuestring;
        if (artist_image_url && cJSON_IsString(artist_image_url)) song.artist_image_url = artist_image_url->valuestring;
        if (album && cJSON_IsString(album)) song.album = album->valuestring;
        if (duration && cJSON_IsNumber(duration)) song.duration = static_cast<int>(duration->valuedouble);

        if (!song.title.empty()) {
            result.push_back(std::move(song));
        }
    }

    cJSON_Delete(root);
    PLOGI << "Loaded " << result.size() << " songs from cache: " << key;
    return result;
}

bool CacheService::hasCache(const std::string& key) const {
    if (!initialized_) return false;
    std::string path = getCachePath(key);
    return std::filesystem::exists(path);
}

bool CacheService::clearCache(const std::string& key) {
    if (!initialized_) return false;
    std::string path = getCachePath(key);
    try {
        if (std::filesystem::exists(path)) {
            std::filesystem::remove(path);
            PLOGI << "Cleared cache: " << key;
            return true;
        }
    } catch (const std::exception& e) {
        PLOGE << "Failed to clear cache: " << e.what();
    }
    return false;
}

bool CacheService::clearAllCache() {
    if (!initialized_) return false;
    try {
        if (std::filesystem::exists(cache_dir_)) {
            for (const auto& entry : std::filesystem::directory_iterator(cache_dir_)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    std::filesystem::remove(entry.path());
                }
            }
            PLOGI << "Cleared all cache";
            return true;
        }
    } catch (const std::exception& e) {
        PLOGE << "Failed to clear all cache: " << e.what();
    }
    return false;
}

}  // namespace ktv::services

