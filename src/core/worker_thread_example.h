/**
 * @file worker_thread_example.h
 * @brief WorkerThread 使用示例
 * 
 * 这是一个完整的使用示例，展示如何基于 WorkerThread 创建 Singleton 线程
 */

#ifndef KTVLV_CORE_WORKER_THREAD_EXAMPLE_H
#define KTVLV_CORE_WORKER_THREAD_EXAMPLE_H

#include "worker_thread.h"
#include <string>
#include <syslog.h>

namespace ktv::core::example {

/**
 * @brief 任务结构体定义
 */
struct NetworkTask {
    enum Type {
        HTTP_REQUEST,
        WEBSOCKET_SEND,
        WEBSOCKET_CLOSE,
    };
    
    Type type;
    std::string url;
    std::string data;
};

/**
 * @brief Network Worker 示例
 */
class NetworkWorker : public WorkerThread<NetworkTask> {
public:
    /**
     * @brief Singleton 访问点
     */
    static NetworkWorker& instance() {
        static NetworkWorker inst;
        return inst;
    }

private:
    /**
     * @brief 私有构造函数
     */
    NetworkWorker() = default;

    /**
     * @brief 私有析构函数
     */
    ~NetworkWorker() override = default;

    /**
     * @brief 禁止拷贝和移动
     */
    NetworkWorker(const NetworkWorker&) = delete;
    NetworkWorker& operator=(const NetworkWorker&) = delete;

    /**
     * @brief 线程启动时调用（资源初始化）
     */
    void onThreadStart() override {
        syslog(LOG_INFO, "[ktv][network] NetworkWorker onThreadStart");
        // 初始化 curl / 连接池 / DNS 缓存等
        // curl_global_init(CURL_GLOBAL_ALL);
    }

    /**
     * @brief 线程停止时调用（资源清理）
     */
    void onThreadStop() override {
        syslog(LOG_INFO, "[ktv][network] NetworkWorker onThreadStop");
        // 清理 curl / 连接池 / DNS 缓存等
        // curl_global_cleanup();
    }

    /**
     * @brief 处理任务（必须实现）
     */
    void processTask(const NetworkTask& task) override {
        syslog(LOG_DEBUG, "[ktv][network] Processing task type=%d, url=%s", 
               task.type, task.url.c_str());
        
        switch (task.type) {
            case NetworkTask::HTTP_REQUEST:
                handleHttpRequest(task);
                break;
            case NetworkTask::WEBSOCKET_SEND:
                handleWebSocketSend(task);
                break;
            case NetworkTask::WEBSOCKET_CLOSE:
                handleWebSocketClose(task);
                break;
        }
    }

    void handleHttpRequest(const NetworkTask& task) {
        // HTTP 请求处理逻辑
        syslog(LOG_INFO, "[ktv][network] HTTP request: %s", task.url.c_str());
    }

    void handleWebSocketSend(const NetworkTask& task) {
        // WebSocket 发送逻辑
        syslog(LOG_INFO, "[ktv][network] WebSocket send: %s", task.data.c_str());
    }

    void handleWebSocketClose(const NetworkTask& task) {
        // WebSocket 关闭逻辑
        syslog(LOG_INFO, "[ktv][network] WebSocket close");
    }
};

/**
 * @brief 使用示例
 */
inline void example_usage() {
    // App 启动时初始化
    NetworkWorker::instance().start();

    // 业务代码中使用
    NetworkTask task;
    task.type = NetworkTask::HTTP_REQUEST;
    task.url = "https://example.com/api";
    task.data = "{}";
    NetworkWorker::instance().post(std::move(task));

    // App 退出时清理
    NetworkWorker::instance().stop();
}

}  // namespace ktv::core::example

#endif  // KTVLV_CORE_WORKER_THREAD_EXAMPLE_H


