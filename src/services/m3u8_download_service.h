#ifndef KTVLV_SERVICES_M3U8_DOWNLOAD_SERVICE_H
#define KTVLV_SERVICES_M3U8_DOWNLOAD_SERVICE_H

#include <string>

namespace ktv::services {

class M3u8DownloadService {
public:
    static M3u8DownloadService& getInstance() {
        static M3u8DownloadService instance;
        return instance;
    }
    M3u8DownloadService(const M3u8DownloadService&) = delete;
    M3u8DownloadService& operator=(const M3u8DownloadService&) = delete;

    bool initialize() { return true; }
    void cleanup() {}

    void startDownload(const std::string& song_id, const std::string& m3u8_url);
    void process();

private:
    M3u8DownloadService() = default;
    ~M3u8DownloadService() = default;
};

}  // namespace ktv::services

#endif  // KTVLV_SERVICES_M3U8_DOWNLOAD_SERVICE_H

