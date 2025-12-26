#include "task_service.h"
#include <lvgl.h>
#include <plog/Log.h>
#include <condition_variable>

namespace ktv::services {

// UI线程回调的包装结构
struct UIThreadCallback {
    std::function<void()> task;
    
    UIThreadCallback(std::function<void()> t) : task(std::move(t)) {}
};

// LVGL异步调用的回调函数
static void lv_async_callback(void* user_data) {
    auto* callback = static_cast<UIThreadCallback*>(user_data);
    if (callback && callback->task) {
        callback->task();
    }
    delete callback;
}

bool TaskService::initialize() {
    if (initialized_.load()) {
        PLOGW << "TaskService already initialized";
        return true;
    }

    running_.store(true);
    worker_thread_ = std::make_unique<std::thread>(&TaskService::workerThread, this);
    
    initialized_.store(true);
    PLOGI << "TaskService initialized, worker thread started";
    return true;
}

void TaskService::cleanup() {
    if (!initialized_.load()) {
        return;
    }

    running_.store(false);
    
    // 唤醒可能正在等待的线程
    {
        std::lock_guard<std::mutex> lock(task_queue_mutex_);
        // 清空队列
        while (!task_queue_.empty()) {
            task_queue_.pop();
        }
    }

    if (worker_thread_ && worker_thread_->joinable()) {
        worker_thread_->join();
        worker_thread_.reset();
    }

    initialized_.store(false);
    PLOGI << "TaskService cleaned up";
}

TaskService::~TaskService() {
    cleanup();
}

void TaskService::runAsync(std::function<void()> task) {
    if (!initialized_.load()) {
        PLOGW << "TaskService not initialized, initializing now...";
        if (!initialize()) {
            PLOGE << "Failed to initialize TaskService";
            return;
        }
    }

    if (!task) {
        PLOGW << "Empty task provided to runAsync";
        return;
    }

    {
        std::lock_guard<std::mutex> lock(task_queue_mutex_);
        task_queue_.push(std::move(task));
    }

    // 注意：这里不使用condition_variable，因为任务队列可能为空
    // 如果后续需要更高效的等待机制，可以添加condition_variable
}

void TaskService::workerThread() {
    PLOGI << "TaskService worker thread started";
    
    while (running_.load()) {
        std::function<void()> task;
        
        // 从队列中取出任务
        {
            std::lock_guard<std::mutex> lock(task_queue_mutex_);
            if (!task_queue_.empty()) {
                task = std::move(task_queue_.front());
                task_queue_.pop();
            }
        }

        if (task) {
            try {
                // 在后台线程执行任务
                task();
            } catch (const std::exception& e) {
                PLOGE << "Exception in background task: " << e.what();
            } catch (...) {
                PLOGE << "Unknown exception in background task";
            }
        } else {
            // 队列为空，短暂休眠避免CPU占用过高
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    PLOGI << "TaskService worker thread stopped";
}

void TaskService::runOnUIThread(std::function<void()> task) {
    if (!task) {
        PLOGW << "Empty task provided to runOnUIThread";
        return;
    }

    // 使用LVGL的异步调用机制，确保在UI线程执行
    // lv_async_call 是线程安全的，可以从任何线程调用
    auto* callback = new UIThreadCallback(std::move(task));
    lv_async_call(lv_async_callback, callback);
}

}  // namespace ktv::services

