#ifndef KTVLV_SERVICES_SONG_SERVICE_H
#define KTVLV_SERVICES_SONG_SERVICE_H

#include <vector>
#include <string>
#include "../config/config.h"

namespace ktv::services {

struct SongItem {
    std::string id;
    std::string title;
    std::string artist;
    std::string m3u8_url;
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

    std::vector<SongItem> listSongs(int page = 1, int size = 20);
    std::vector<SongItem> search(const std::string& keyword, int page = 1, int size = 20);
    bool addToQueue(const std::string& song_id);

private:
    SongService() = default;
    ~SongService() = default;

    std::string token_;
    ktv::config::NetworkConfig net_cfg_;
};

}  // namespace ktv::services

#endif  // KTVLV_SERVICES_SONG_SERVICE_H

