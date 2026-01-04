/**
 * @file worker_thread.h
 * @brief Singleton Worker 线程模板（基于 ThreadBase）
 * 
 * 使用方式：
 * 1. 定义 Task 结构体
 * 2. 继承 WorkerThread<YourTask>
 * 3. 实现 processTask() 方法
 * 4. 实现 instance() 静态方法（Singleton 模式）
 * 5. 在 App 启动时调用 instance().start()
 * 6. 在 App 退出时调用 instance().stop()
 * 
 * 示例：
 * ```cpp
 * struct MyTask {
 *     int type;
 *     std::string data;
 * };
 * 
 * class MyWorker : public WorkerThread<MyTask> {
 * public:
 *     static MyWorker& instance() {
 *         static MyWorker inst;
 *         return inst;
 *     }
 * 
 * private:
 *     MyWorker() = default;
 *     ~MyWorker() = default;
 *     MyWorker(const MyWorker&) = delete;
 *     MyWorker& operator=(const MyWorker&) = delete;
 * 
 *     void processTask(const MyTask& task) override {
 *         // 处理任务
 *     }
 * };
 * ```
 */

#ifndef KTVLV_CORE_WORKER_THREAD_H
#define KTVLV_CORE_WORKER_THREAD_H

#include "thread_base.h"

namespace ktv::core {

/**
 * @brief Singleton Worker 线程模板
 * 
 * 基于 ThreadBase，添加 Singleton 模式支持
 */
template<typename TaskType>
class WorkerThread : public ThreadBase<TaskType> {
public:
    // 禁止拷贝和移动
    WorkerThread(const WorkerThread&) = delete;
    WorkerThread& operator=(const WorkerThread&) = delete;
    WorkerThread(WorkerThread&&) = delete;
    WorkerThread& operator=(WorkerThread&&) = delete;

protected:
    /**
     * @brief 受保护的构造函数（子类必须实现 Singleton）
     */
    WorkerThread() = default;

    /**
     * @brief 受保护的析构函数
     */
    virtual ~WorkerThread() = default;
};

}  // namespace ktv::core

#endif  // KTVLV_CORE_WORKER_THREAD_H

