#include "m3u8_download_service.h"
#include <syslog.h>
#include "../events/event_bus.h"
#include <chrono>

namespace ktv::services {

bool M3u8DownloadService::initialize() {
    ensureThreadStarted();
    return true;
}

void M3u8DownloadService::cleanup() {
    stopThread();
}

void M3u8DownloadService::startDownload(const std::string& song_id, const std::string& m3u8_url) {
    // MVP 简化：下载线程串行处理下载任务
    ensureThreadStarted();
    {
        std::lock_guard<std::mutex> lock(mtx_);
        queue_.push(Task{song_id, m3u8_url});
    }
    cv_.notify_one();
    syslog(LOG_INFO, "[ktv][download][enqueue] song_id=%s url=%s", song_id.c_str(), m3u8_url.c_str());
}

void M3u8DownloadService::ensureThreadStarted() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return; // already running
    }
    worker_ = std::thread([this] { threadLoop(); });
    syslog(LOG_INFO, "[ktv][download][thread] status=started");
}

void M3u8DownloadService::stopThread() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) {
        return; // not running
    }
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
    syslog(LOG_INFO, "[ktv][download][thread] status=stopped");
}

void M3u8DownloadService::threadLoop() {
    while (running_) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this] { return !running_ || !queue_.empty(); });
            if (!running_) break;
            task = std::move(queue_.front());
            queue_.pop();
        }

        // TODO: 真实实现：下载 m3u8、解析 ts 列表、顺序下载 ts、写入缓存目录
        // MVP阶段：先用模拟流程验证“DownloadThread → EventBus → UI主线程”闭环
        syslog(LOG_INFO, "[ktv][download][start] song_id=%s", task.song_id.c_str());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        ktv::events::Event ev;
        ev.type = ktv::events::EventType::DownloadCompleted;
        ev.payload = task.song_id;
        ktv::events::EventBus::getInstance().publish(ev);
        syslog(LOG_INFO, "[ktv][download][done] song_id=%s status=mock", task.song_id.c_str());
    }
}

}  // namespace ktv::services

