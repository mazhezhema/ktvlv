// log_upload_service.cpp
// LogUploadService 实现
#include "log_upload_service.h"
#include "http_service.h"
#include <syslog.h>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <thread>

namespace ktv::services {

LogUploadService& LogUploadService::instance() {
    static LogUploadService inst;
    return inst;
}

LogUploadService::LogUploadService() {
    // TODO: 从配置服务获取设备信息
    device_id_ = "F133-001";
    fw_version_ = "1.0.0";
    
    openlog("ktv", LOG_PID, LOG_USER);
}

LogUploadService::~LogUploadService() {
    stop();
    closelog();
}

void LogUploadService::start() {
    if (running_.exchange(true)) {
        return;  // 已经启动
    }
    
    worker_ = std::thread([this] {
        threadLoop();
    });
    
    syslog(LOG_INFO, "[ktv][log] LogUploadService started");
}

void LogUploadService::stop() {
    if (!running_.exchange(false)) {
        return;  // 已经停止
    }
    
    // 唤醒等待的线程
    {
        std::lock_guard<std::mutex> lock(queue_mtx_);
        queue_cv_.notify_all();
    }
    
    if (worker_.joinable()) {
        worker_.join();
    }
    
    syslog(LOG_INFO, "[ktv][log] LogUploadService stopped");
}

void LogUploadService::notify(UploadReason reason) {
    {
        std::lock_guard<std::mutex> lock(queue_mtx_);
        
        // 触发合并：10分钟内多次触发合并为一次
        auto now = std::chrono::steady_clock::now();
        if (last_trigger_time_.time_since_epoch().count() > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - last_trigger_time_).count();
            if (elapsed < TRIGGER_MERGE_SECONDS) {
                // 10分钟内已有触发，跳过
                syslog(LOG_DEBUG, "[ktv][log] trigger merged, reason=%d", (int)reason);
                return;
            }
        }
        
        task_queue_.push(reason);
        last_trigger_time_ = now;
    }
    
    queue_cv_.notify_one();
    syslog(LOG_INFO, "[ktv][log] upload triggered, reason=%d", (int)reason);
}

bool LogUploadService::exportToFile(const char* path) {
    std::string logs;
    if (!collectLogs(logs)) {
        syslog(LOG_ERR, "[ktv][log] export failed: collect logs failed");
        return false;
    }
    
    FILE* fp = fopen(path, "w");
    if (!fp) {
        syslog(LOG_ERR, "[ktv][log] export failed: cannot open file %s", path);
        return false;
    }
    
    fwrite(logs.data(), 1, logs.size(), fp);
    fclose(fp);
    
    syslog(LOG_INFO, "[ktv][log] export success: %s", path);
    return true;
}

void LogUploadService::threadLoop() {
    while (running_) {
        // 状态机主循环
        {
            std::lock_guard<std::mutex> lock(state_mtx_);
            switch (state_) {
            case State::IDLE:
                handleIdle();
                break;
            case State::COLLECTING:
                handleCollecting();
                break;
            case State::PACKING:
                handlePacking();
                break;
            case State::UPLOADING:
                handleUploading();
                break;
            case State::BACKOFF:
                handleBackoff();
                break;
            }
        }
        
        // 短暂休眠，避免 CPU 占用过高
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void LogUploadService::handleIdle() {
    // 检查是否有任务
    {
        std::unique_lock<std::mutex> lock(queue_mtx_);
        if (task_queue_.empty()) {
            // 等待任务或超时
            queue_cv_.wait_for(lock, std::chrono::seconds(1));
            if (task_queue_.empty()) {
                return;  // 无任务，继续等待
            }
        }
        
        // 有任务，进入 COLLECTING
        state_ = State::COLLECTING;
    }
}

void LogUploadService::handleCollecting() {
    std::string logs;
    if (!collectLogs(logs)) {
        syslog(LOG_ERR, "[ktv][log] collect logs failed");
        state_ = State::IDLE;
        return;
    }
    
    if (logs.empty()) {
        syslog(LOG_INFO, "[ktv][log] no logs to upload");
        state_ = State::IDLE;
        return;
    }
    
    // 保存日志，进入 PACKING
    collected_logs_ = std::move(logs);
    state_ = State::PACKING;
}

void LogUploadService::handlePacking() {
    std::string payload = buildPayload(collected_logs_);
    if (payload.empty()) {
        syslog(LOG_ERR, "[ktv][log] build payload failed");
        state_ = State::IDLE;
        return;
    }
    
    // 保存 payload，进入 UPLOADING
    upload_payload_ = std::move(payload);
    state_ = State::UPLOADING;
}

void LogUploadService::handleUploading() {
    if (!uploadLogs(upload_payload_)) {
        syslog(LOG_ERR, "[ktv][log] upload failed");
        enterBackoff();
        return;
    }
    
    syslog(LOG_INFO, "[ktv][log] upload success");
    
    // 清理
    collected_logs_.clear();
    upload_payload_.clear();
    
    // 回到 IDLE
    state_ = State::IDLE;
}

void LogUploadService::handleBackoff() {
    auto now = std::chrono::steady_clock::now();
    if (now < backoff_until_) {
        // 还在退避期，继续等待
        return;
    }
    
    // 退避期结束，回到 IDLE
    exitBackoff();
    state_ = State::IDLE;
}

bool LogUploadService::collectLogs(std::string& out) {
    out.clear();
    
    FILE* fp = popen("logread", "r");
    if (!fp) {
        return false;
    }
    
    char line[512];
    size_t lines = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        // 过滤关键词
        if (config_.include_keyword && 
            std::strstr(line, config_.include_keyword) == nullptr) {
            continue;
        }
        
        size_t len = std::strlen(line);
        
        // 检查字节上限
        if (out.size() + len > config_.max_bytes) {
            out.append("[ktv][log][truncate] reason=max_bytes\n");
            break;
        }
        
        out.append(line);
        lines++;
        
        // 检查行数上限
        if (lines >= config_.max_lines) {
            out.append("[ktv][log][truncate] reason=max_lines\n");
            break;
        }
    }
    
    int ret = pclose(fp);
    if (ret != 0) {
        return false;
    }
    
    return true;
}

std::string LogUploadService::buildPayload(const std::string& logs) {
    // JSON 转义函数
    auto escapeJson = [](const std::string& s) -> std::string {
        std::string o;
        o.reserve(s.size() + 64);
        for (char c : s) {
            switch (c) {
            case '\\': o += "\\\\"; break;
            case '"':  o += "\\\""; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:   o += c;      break;
            }
        }
        return o;
    };
    
    // 计算运行时间（简化实现：使用当前时间戳）
    // TODO: 从系统获取真实 uptime（/proc/uptime）
    long uptime_sec = std::time(nullptr);  // 临时使用时间戳
    
    // 构建 JSON
    std::ostringstream oss;
    oss << "{"
        << "\"device_id\":\"" << device_id_ << "\","
        << "\"fw_version\":\"" << fw_version_ << "\","
        << "\"uptime\":" << uptime_sec << ","
        << "\"logs\":\"" << escapeJson(logs) << "\""
        << "}";
    
    return oss.str();
}

bool LogUploadService::uploadLogs(const std::string& payload) {
    // 使用 HttpService 上传
    HttpResponse response;
    bool success = HttpService::getInstance().post(
        "/api/logs/upload", 
        payload.c_str(), 
        response);
    
    if (!success) {
        return false;
    }
    
    // 检查响应状态码
    if (response.status_code != 200) {
        return false;
    }
    
    return true;
}

void LogUploadService::enterBackoff() {
    // 指数退避：10s, 30s, 60s
    if (backoff_seconds_ == 0) {
        backoff_seconds_ = 10;
    } else if (backoff_seconds_ < 60) {
        backoff_seconds_ = std::min(backoff_seconds_ * 3, 60);
    }
    
    backoff_until_ = std::chrono::steady_clock::now() + 
                     std::chrono::seconds(backoff_seconds_);
    
    syslog(LOG_WARNING, "[ktv][log] enter backoff, seconds=%d", backoff_seconds_);
    state_ = State::BACKOFF;
}

void LogUploadService::exitBackoff() {
    backoff_seconds_ = 0;
    syslog(LOG_INFO, "[ktv][log] exit backoff");
}

}  // namespace ktv::services

