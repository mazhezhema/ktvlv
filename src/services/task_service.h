#ifndef KTVLV_SERVICES_TASK_SERVICE_H
#define KTVLV_SERVICES_TASK_SERVICE_H

#include <functional>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <memory>

namespace ktv::services {

/**
 * TaskService - 异步任务调度服务
 * 
 * 核心原则：
 * - UI线程不做耗时操作
 * - 所有耗时任务（网络、IO、JSON解析）进入后台线程
 * - 完成后通过回调回到UI线程更新界面
 * 
 * 使用方式：
 * ```cpp
 * TaskService::getInstance().runAsync([](){
 *     // 后台线程执行：网络请求、IO操作等
 *     auto data = fetchData();
 *     
 *     // 完成后回到UI线程更新界面
 *     TaskService::getInstance().runOnUIThread([data](){
 *         updateUI(data);
 *     });
 * });
 * ```
 */
class TaskService {
public:
    static TaskService& getInstance() {
        static TaskService instance;
        return instance;
    }
    TaskService(const TaskService&) = delete;
    TaskService& operator=(const TaskService&) = delete;

    // 初始化服务（启动后台线程）
    bool initialize();
    
    // 清理资源（停止后台线程）
    void cleanup();

    /**
     * 在后台线程执行任务（异步，立即返回）
     * @param task 要执行的任务函数
     */
    void runAsync(std::function<void()> task);

    /**
     * 在UI线程执行任务（通过LVGL的异步调用机制）
     * 注意：必须在LVGL初始化后调用
     * @param task 要执行的任务函数
     */
    void runOnUIThread(std::function<void()> task);

    // 检查是否已初始化
    bool isInitialized() const { return initialized_.load(); }

private:
    TaskService() = default;
    ~TaskService();

    // 后台线程主循环
    void workerThread();

    std::queue<std::function<void()>> task_queue_;
    std::mutex task_queue_mutex_;
    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};
    std::unique_ptr<std::thread> worker_thread_;
};

}  // namespace ktv::services

#endif  // KTVLV_SERVICES_TASK_SERVICE_H

