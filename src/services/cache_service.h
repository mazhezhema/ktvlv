#ifndef KTVLV_SERVICES_CACHE_SERVICE_H
#define KTVLV_SERVICES_CACHE_SERVICE_H

#include <vector>
#include <string>
#include "song_service.h"

namespace ktv::services {

// 缓存服务：负责数据的本地持久化和加载
// 离线优先策略：先读缓存，联网后更新并保存
class CacheService {
public:
    static CacheService& getInstance() {
        static CacheService instance;
        return instance;
    }
    CacheService(const CacheService&) = delete;
    CacheService& operator=(const CacheService&) = delete;

    // 初始化缓存目录
    bool initialize(const std::string& cache_dir = "cache");

    // 保存歌曲列表到缓存
    bool saveSongs(const std::string& key, const std::vector<SongItem>& songs);
    
    // 从缓存加载歌曲列表
    std::vector<SongItem> loadSongs(const std::string& key);
    
    // 检查缓存是否存在
    bool hasCache(const std::string& key) const;
    
    // 清除指定缓存
    bool clearCache(const std::string& key);
    
    // 清除所有缓存
    bool clearAllCache();

private:
    CacheService() = default;
    ~CacheService() = default;

    std::string cache_dir_;
    bool initialized_ = false;
    
    std::string getCachePath(const std::string& key) const;
};

}  // namespace ktv::services

#endif  // KTVLV_SERVICES_CACHE_SERVICE_H

