#ifndef KTVLV_SERVICES_M3U8_DOWNLOAD_SERVICE_H
#define KTVLV_SERVICES_M3U8_DOWNLOAD_SERVICE_H

#include <string>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

namespace ktv::services {

class M3u8DownloadService {
public:
    static M3u8DownloadService& getInstance() {
        static M3u8DownloadService instance;
        return instance;
    }
    M3u8DownloadService(const M3u8DownloadService&) = delete;
    M3u8DownloadService& operator=(const M3u8DownloadService&) = delete;

    /**
     * MVP 简化架构：唯一允许的后台线程（Download Thread）
     * - 串行执行 m3u8/ts 下载任务
     * - 通过 EventBus（UiEventQueue）回到 UI 主线程更新状态
     */
    bool initialize();
    void cleanup();

    void startDownload(const std::string& song_id, const std::string& m3u8_url);

private:
    M3u8DownloadService() = default;
    ~M3u8DownloadService() = default;

    struct Task {
        std::string song_id;
        std::string m3u8_url;
    };

    void ensureThreadStarted();
    void stopThread();
    void threadLoop();

    std::atomic<bool> running_{false};
    std::thread worker_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<Task> queue_;
};

}  // namespace ktv::services

#endif  // KTVLV_SERVICES_M3U8_DOWNLOAD_SERVICE_H

