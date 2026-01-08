/**
 * @file thread_base.h
 * @brief Singleton 线程基类模板（Tina / F133 / KTV 标准模板）
 * 
 * 核心原则：
 * - Singleton 模式（宿主对象全局唯一）
 * - std::thread 成员变量（不在构造函数中启动）
 * - 阻塞等待（condition_variable，零 busy-loop）
 * - 显式 start/stop（由 App 主流程控制）
 * - 显式 join（确保线程安全退出）
 * 
 * 使用方式：
 * 1. 继承 ThreadBase
 * 2. 实现 processTask() 方法
 * 3. 可选：重写 onThreadStart() 和 onThreadStop()
 * 4. 在 App 启动时调用 instance().start()
 * 5. 在 App 退出时调用 instance().stop()
 */

#ifndef KTVLV_CORE_THREAD_BASE_H
#define KTVLV_CORE_THREAD_BASE_H

#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <syslog.h>

namespace ktv::core {

/**
 * @brief Singleton 线程基类模板
 * 
 * 特点：
 * - Singleton 模式
 * - std::thread 成员变量
 * - 阻塞等待（condition_variable）
 * - 显式 start/stop
 * - 显式 join
 */
template<typename TaskType>
class ThreadBase {
public:
    virtual ~ThreadBase() {
        if (running_.load()) {
            syslog(LOG_ERR, "[ktv][thread] ThreadBase destroyed without stop()!");
            stop();
        }
    }

    /**
     * @brief 启动线程（必须在 App 启动时显式调用）
     */
    void start() {
        bool expected = false;
        if (!running_.compare_exchange_strong(expected, true)) {
            syslog(LOG_WARNING, "[ktv][thread] ThreadBase already started");
            return;
        }
        
        worker_ = std::thread(&ThreadBase::threadLoop, this);
        syslog(LOG_INFO, "[ktv][thread] ThreadBase started");
    }

    /**
     * @brief 停止线程（必须在 App 退出时显式调用）
     */
    void stop() {
        if (!running_.exchange(false)) {
            syslog(LOG_WARNING, "[ktv][thread] ThreadBase already stopped");
            return;
        }
        
        cv_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
        
        syslog(LOG_INFO, "[ktv][thread] ThreadBase stopped");
    }

    /**
     * @brief 投递任务到队列
     */
    void post(TaskType task) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            tasks_.push(std::move(task));
        }
        cv_.notify_one();
    }

protected:
    /**
     * @brief 线程启动时调用（资源初始化）
     * 子类可以重写此方法进行资源初始化
     */
    virtual void onThreadStart() {
        syslog(LOG_DEBUG, "[ktv][thread] ThreadBase onThreadStart");
    }

    /**
     * @brief 线程停止时调用（资源清理）
     * 子类可以重写此方法进行资源清理
     */
    virtual void onThreadStop() {
        syslog(LOG_DEBUG, "[ktv][thread] ThreadBase onThreadStop");
    }

    /**
     * @brief 处理任务（子类必须实现）
     */
    virtual void processTask(const TaskType& task) = 0;

    /**
     * @brief 获取运行状态
     */
    bool isRunning() const {
        return running_.load();
    }

private:
    void threadLoop() {
        syslog(LOG_INFO, "[ktv][thread] ThreadBase loop started");
        
        onThreadStart();

        while (running_.load()) {
            TaskType task;
            
            {
                std::unique_lock<std::mutex> lock(mtx_);
                cv_.wait(lock, [this]() {
                    return !tasks_.empty() || !running_.load();
                });

                if (!running_.load()) {
                    break;
                }

                if (tasks_.empty()) {
                    continue;
                }

                task = std::move(tasks_.front());
                tasks_.pop();
            }

            try {
                processTask(task);
            } catch (const std::exception& e) {
                syslog(LOG_ERR, "[ktv][thread] Task execution failed: %s", e.what());
            }
        }

        onThreadStop();
        syslog(LOG_INFO, "[ktv][thread] ThreadBase loop exited");
    }

protected:
    std::thread worker_;
    std::atomic<bool> running_{false};

private:
    std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<TaskType> tasks_;
};

}  // namespace ktv::core

#endif  // KTVLV_CORE_THREAD_BASE_H


