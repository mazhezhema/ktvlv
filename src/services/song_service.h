#ifndef KTVLV_SERVICES_SONG_SERVICE_H
#define KTVLV_SERVICES_SONG_SERVICE_H

#include <vector>
#include <string>
#include <functional>
#include "../config/config.h"

namespace ktv::services {

struct SongItem {
    std::string id;
    std::string title;
    std::string artist;
    std::string m3u8_url;
    std::string cover_url;      // 封面图片URL
    std::string artist_image_url; // 歌手图片URL
    std::string album;          // 专辑名称
    int duration = 0;           // 时长（秒）
    
    // 默认构造函数
    SongItem() = default;
    
    // 便捷构造函数（兼容旧代码）
    SongItem(const std::string& id, const std::string& title, const std::string& artist)
        : id(id), title(title), artist(artist) {}
};

class SongService {
public:
    static SongService& getInstance() {
        static SongService instance;
        return instance;
    }
    SongService(const SongService&) = delete;
    SongService& operator=(const SongService&) = delete;

    void setToken(const std::string& token) { token_ = token; }
    const std::string& getToken() const { return token_; }
    void setNetworkConfig(const ktv::config::NetworkConfig& cfg) { net_cfg_ = cfg; }
    const ktv::config::NetworkConfig& getNetworkConfig() const { return net_cfg_; }

    // 原始方法：直接联网请求（同步）
    std::vector<SongItem> listSongs(int page = 1, int size = 20);
    std::vector<SongItem> search(const std::string& keyword, int page = 1, int size = 20);
    bool addToQueue(const std::string& song_id);
    
    // 离线优先方法：先读缓存，再尝试联网更新
    // 返回缓存的歌曲列表（如果有），联网成功后会更新缓存
    std::vector<SongItem> listSongsOfflineFirst(int page = 1, int size = 20);
    std::vector<SongItem> searchOfflineFirst(const std::string& keyword, int page = 1, int size = 20);
    
    // 异步版本：在后台线程执行，完成后通过回调更新UI
    // callback: (const std::vector<SongItem>& songs) -> void
    void listSongsOfflineFirstAsync(int page, int size, std::function<void(const std::vector<SongItem>&)> callback);
    void searchOfflineFirstAsync(const std::string& keyword, int page, int size, std::function<void(const std::vector<SongItem>&)> callback);

private:
    SongService() = default;
    ~SongService() = default;

    std::string token_;
    ktv::config::NetworkConfig net_cfg_;
};

}  // namespace ktv::services

#endif  // KTVLV_SERVICES_SONG_SERVICE_H

