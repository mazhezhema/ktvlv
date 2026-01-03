// log_upload_service.h
// LogUploadService - 日志上传服务（F133/Tina Linux）
#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace ktv::services {

/**
 * 上传原因枚举
 */
enum class UploadReason {
    PLAYER_ERROR,      // 播放失败
    NETWORK_ERROR,     // 连续网络错误
    USER_FEEDBACK,     // 用户反馈
    ADMIN_COMMAND,     // 运维指令
    PERIODIC           // 定时上传（可选，MVP暂不启用）
};

/**
 * LogUploadService - 日志上传服务
 * 
 * 设计原则：
 * 1. 独立后台线程（低优先级）
 * 2. 按需触发，不实时上传
 * 3. 数据上限保护（256KB/2000行）
 * 4. 失败进入 backoff（指数退避）
 * 5. 不影响 UI/播放
 * 
 * 使用方式：
 * - 各模块只调用 Notify(reason)，不传日志内容
 * - 服务线程负责 logread 抓取、过滤、打包、上传
 */
class LogUploadService {
public:
    static LogUploadService& instance();
    
    // 禁止拷贝
    LogUploadService(const LogUploadService&) = delete;
    LogUploadService& operator=(const LogUploadService&) = delete;
    
    /**
     * 启动服务线程
     */
    void start();
    
    /**
     * 停止服务线程
     */
    void stop();
    
    /**
     * 通知上传（非阻塞）
     * @param reason 上传原因
     */
    void notify(UploadReason reason);
    
    /**
     * 一键导出到文件（可选，售后工具）
     * @param path 文件路径
     * @return true 成功，false 失败
     */
    bool exportToFile(const char* path);
    
private:
    LogUploadService();
    ~LogUploadService();
    
    // 线程主循环
    void threadLoop();
    
    // 状态机
    enum class State {
        IDLE,          // 空闲，等待触发
        COLLECTING,    // 收集日志中
        PACKING,       // 打包中
        UPLOADING,     // 上传中
        BACKOFF        // 退避中
    };
    
    // 状态机处理
    void handleIdle();
    void handleCollecting();
    void handlePacking();
    void handleUploading();
    void handleBackoff();
    
    // 辅助函数
    bool collectLogs(std::string& out);
    std::string buildPayload(const std::string& logs);
    bool uploadLogs(const std::string& payload);
    void enterBackoff();
    void exitBackoff();
    
    // 配置
    struct Config {
        size_t max_bytes = 256 * 1024;  // 256KB
        size_t max_lines = 2000;
        const char* include_keyword = "[ktv]";
        int upload_timeout_sec = 5;
        int max_retries = 2;
    } config_;
    
    // 设备信息（TODO: 从配置服务获取）
    std::string device_id_;
    std::string fw_version_;
    
    // 线程控制
    std::thread worker_;
    std::atomic<bool> running_{false};
    
    // 任务队列
    std::queue<UploadReason> task_queue_;
    std::mutex queue_mtx_;
    std::condition_variable queue_cv_;
    
    // 状态机
    State state_{State::IDLE};
    std::mutex state_mtx_;
    
    // Backoff 控制
    int backoff_seconds_{0};
    std::chrono::steady_clock::time_point backoff_until_;
    
    // 触发合并（10分钟内多次触发合并为一次）
    std::chrono::steady_clock::time_point last_trigger_time_;
    static constexpr int TRIGGER_MERGE_SECONDS = 600;  // 10分钟
    
    // 状态机临时数据
    std::string collected_logs_;   // COLLECTING -> PACKING 传递
    std::string upload_payload_;   // PACKING -> UPLOADING 传递
};

}  // namespace ktv::services

