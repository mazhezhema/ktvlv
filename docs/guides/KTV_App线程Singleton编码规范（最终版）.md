# KTV App 线程 Singleton 编码规范（最终版）

> **文档版本**：v1.0  
> **最后更新**：2025-12-30  
> **状态**：✅ 核心文档（工程规范 - 铁律级）  
> **适用平台**：F133 / Tina Linux  
> **目标**：为工程师提供可复制的模板和铁律级避坑指南

---

## 🎯 核心结论（一句话版）

> **Singleton 线程的最佳实践 = "宿主对象 Singleton + std::thread 常驻 + 阻塞等待 + 显式 stop + 显式 join"**

任何偏离这 5 点的实现，**迟早出事**。

---

## 📋 目录

1. [标准推荐架构](#一-标准推荐架构)
2. [黄金模板](#二-黄金模板推荐统一使用)
3. [为什么这套模板是"最稳"的](#三-为什么这套模板是最稳的)
4. [避坑指南（铁律级禁止项）](#四-避坑指南铁律级禁止项)
5. [工程铁律（必须遵守）](#五-工程铁律必须遵守)
6. [适用线程列表](#六-适用线程列表)

---

## 一、标准推荐架构

### 架构图

```
Singleton Thread Object
├── std::thread           （只在 start() 创建）
├── std::atomic<bool>     running / exiting
├── std::mutex
├── std::condition_variable
├── std::queue<Task>
└── loop()                （while(running) + 阻塞）
```

### 关键原则

- ✅ **Singleton ≠ static thread**
- ✅ **thread 是成员，不是 static 全局变量**
- ✅ **构造只做轻量初始化，不启动线程**
- ✅ **线程在 start() 中创建，在 stop() 中销毁**
- ✅ **所有线程必须阻塞等待，不允许 busy-loop**

---

## 二、黄金模板（推荐统一使用）

这是 **Tina / F133 / KTV** 场景下的标准模板，**所有 Singleton 线程都应该遵循这个模式**。

### 完整模板代码

```cpp
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <syslog.h>

/**
 * @brief Singleton Worker 线程标准模板
 * 
 * 特点：
 * - Singleton 模式（宿主对象全局唯一）
 * - std::thread 成员变量（不在构造函数中启动）
 * - 阻塞等待（condition_variable，零 busy-loop）
 * - 显式 start/stop（由 App 主流程控制）
 * - 显式 join（确保线程安全退出）
 */
class SingletonWorker {
public:
    struct Task {
        int type;
        void* data;
        
        void run() {
            // 任务执行逻辑（子类实现）
        }
    };

    // Singleton 访问点
    static SingletonWorker& instance() {
        static SingletonWorker inst;
        return inst;
    }

    // 禁止拷贝和移动
    SingletonWorker(const SingletonWorker&) = delete;
    SingletonWorker& operator=(const SingletonWorker&) = delete;
    SingletonWorker(SingletonWorker&&) = delete;
    SingletonWorker& operator=(SingletonWorker&&) = delete;

    /**
     * @brief 启动线程（必须在 App 启动时显式调用）
     * 
     * 关键点：
     * - 使用 compare_exchange_strong 确保只启动一次
     * - 不在构造函数中启动（避免静态初始化顺序问题）
     */
    void start() {
        bool expected = false;
        if (!running_.compare_exchange_strong(expected, true)) {
            syslog(LOG_WARNING, "[ktv][thread] SingletonWorker already started");
            return; // already running
        }
        
        worker_ = std::thread(&SingletonWorker::threadLoop, this);
        syslog(LOG_INFO, "[ktv][thread] SingletonWorker started");
    }

    /**
     * @brief 停止线程（必须在 App 退出时显式调用）
     * 
     * 关键点：
     * - 设置 running_ = false
     * - 唤醒所有等待的线程
     * - 显式 join（确保线程安全退出）
     */
    void stop() {
        if (!running_.exchange(false)) {
            syslog(LOG_WARNING, "[ktv][thread] SingletonWorker already stopped");
            return; // already stopped
        }
        
        cv_.notify_all();  // 唤醒所有等待的线程
        
        if (worker_.joinable()) {
            worker_.join();  // 显式 join，确保线程安全退出
        }
        
        syslog(LOG_INFO, "[ktv][thread] SingletonWorker stopped");
    }

    /**
     * @brief 投递任务到队列
     */
    void post(Task task) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            tasks_.push(std::move(task));
        }
        cv_.notify_one();  // 唤醒一个等待的线程
    }

private:
    /**
     * @brief 私有构造函数（只做轻量初始化）
     * 
     * 关键点：
     * - 不启动线程
     * - 不做耗时操作
     * - 不依赖其他 Singleton
     */
    SingletonWorker() = default;

    /**
     * @brief 私有析构函数（不允许自动析构）
     * 
     * 关键点：
     * - 不允许自动析构
     * - 必须显式调用 stop()
     */
    ~SingletonWorker() {
        // 不允许自动析构，必须显式调用 stop()
        if (running_.load()) {
            syslog(LOG_ERR, "[ktv][thread] SingletonWorker destroyed without stop()!");
            // 紧急清理（不推荐，但总比崩溃好）
            stop();
        }
    }

    /**
     * @brief 线程主循环（核心实现）
     * 
     * 关键点：
     * - 在 onThreadStart() 中初始化资源
     * - 使用 condition_variable 阻塞等待（零 busy-loop）
     * - 在 onThreadStop() 中清理资源
     */
    void threadLoop() {
        syslog(LOG_INFO, "[ktv][thread] SingletonWorker loop started");
        
        // 在线程内初始化资源（避免构造函数中做重活）
        onThreadStart();

        while (running_.load()) {
            Task task;
            
            // 阻塞等待任务（关键：不 busy-loop）
            {
                std::unique_lock<std::mutex> lock(mtx_);
                cv_.wait(lock, [this]() {
                    return !tasks_.empty() || !running_.load();
                });

                // 检查是否需要退出
                if (!running_.load()) {
                    break;
                }

                // 队列为空（异常情况，应该不会发生）
                if (tasks_.empty()) {
                    continue;
                }

                // 取出任务（在锁内）
                task = std::move(tasks_.front());
                tasks_.pop();
            }

            // 处理任务（在锁外执行，避免长时间持锁）
            try {
                task.run();
            } catch (const std::exception& e) {
                syslog(LOG_ERR, "[ktv][thread] Task execution failed: %s", e.what());
            }
        }

        // 清理资源
        onThreadStop();
        
        syslog(LOG_INFO, "[ktv][thread] SingletonWorker loop exited");
    }

    /**
     * @brief 线程启动时调用（资源初始化）
     * 
     * 关键点：
     * - 在线程内执行，不在构造函数中
     * - 可以安全访问其他 Singleton
     * - 可以执行耗时操作
     */
    virtual void onThreadStart() {
        // 子类实现：初始化 curl / player / sqlite / log tag
        syslog(LOG_DEBUG, "[ktv][thread] SingletonWorker onThreadStart");
    }

    /**
     * @brief 线程停止时调用（资源清理）
     * 
     * 关键点：
     * - 在线程内执行，不在析构函数中
     * - 可以安全访问其他 Singleton
     * - 必须清理所有分配的资源
     */
    virtual void onThreadStop() {
        // 子类实现：清理资源
        syslog(LOG_DEBUG, "[ktv][thread] SingletonWorker onThreadStop");
    }

private:
    std::thread worker_;                    // 线程对象（成员变量，不是 static）
    std::mutex mtx_;                        // 互斥锁
    std::condition_variable cv_;            // 条件变量（阻塞等待）
    std::queue<Task> tasks_;                // 任务队列
    std::atomic<bool> running_{false};      // 运行标志（原子变量）
};
```

### 使用示例

```cpp
// App 启动时初始化
void app_main_init() {
    // 所有 Singleton 线程统一启动
    SingletonWorker::instance().start();
    NetworkWorker::instance().start();
    PlayerAdapter::instance().start();
    LogUploadService::instance().start();
}

// 业务代码中使用
void sendTask() {
    SingletonWorker::Task task;
    task.type = TASK_TYPE_XXX;
    task.data = /* ... */;
    SingletonWorker::instance().post(std::move(task));
}

// App 退出时清理（按依赖顺序）
void app_main_cleanup() {
    // 按依赖顺序停止（依赖者先停止）
    SingletonWorker::instance().stop();
    NetworkWorker::instance().stop();
    PlayerAdapter::instance().stop();
    LogUploadService::instance().stop();
}
```

---

## 三、为什么这套模板是"最稳"的

### ✅ 1. Singleton 只管"唯一性"，不管启动

- ✅ 不在 `instance()` 里隐式 start
- ✅ 启动顺序 **由 App 明确控制**
- ✅ 避免静态初始化顺序灾难

### ✅ 2. std::thread 生命周期 100% 可控

- ✅ 创建：start()
- ✅ 结束：stop() + join()
- ✅ 没有 detach
- ✅ 没有悬空线程

### ✅ 3. 阻塞等待，零 busy-loop

- ✅ 使用 `condition_variable`
- ✅ CPU 安静
- ✅ 电源稳定
- ✅ 行为可预测

### ✅ 4. 所有退出路径都能收敛

- ✅ App 正常退出
- ✅ App 异常退出
- ✅ Watchdog kill 前

### ✅ 5. 资源初始化/清理位置明确

- ✅ 初始化：onThreadStart()（在线程内）
- ✅ 清理：onThreadStop()（在线程内）
- ✅ 不在构造函数/析构函数中做重活

---

## 四、避坑指南（铁律级禁止项）

下面这些 **可以直接写成"禁止项"**，违反即视为代码审查不通过。

---

### 🚫 坑 1：在 Singleton 构造函数里启动线程

```cpp
// ❌ 错误：在构造函数中启动线程
SingletonWorker::SingletonWorker() {
    worker_ = std::thread(&SingletonWorker::threadLoop, this);  // 错误！
}

// ❌ 错误：在 instance() 中隐式启动
static SingletonWorker& instance() {
    static SingletonWorker inst;
    inst.start();  // 错误！隐式启动
    return inst;
}
```

**为什么是大坑？**

1. **构造顺序不可控**：如果 A::instance() 在构造时调用 B::instance()，可能导致死锁
2. **依赖的其他 Singleton 可能还没 ready**：初始化顺序不确定
3. **gdb 根本不好调**：静态初始化难以调试
4. **测试困难**：难以控制初始化顺序

✅ **正确做法**：

```cpp
// ✅ 正确：构造只做轻量初始化
SingletonWorker::SingletonWorker() = default;

// ✅ 正确：start() 由 App 主流程显式调用
void app_main_init() {
    SingletonWorker::instance().start();  // 正确！
}
```

---

### 🚫 坑 2：static std::thread 全局对象

```cpp
// ❌ 错误：static thread 全局对象
static std::thread g_worker;

void initWorker() {
    g_worker = std::thread(workerLoop);  // 错误！
}
```

**后果：**

1. **析构顺序不可控**：C++ 静态对象析构顺序不确定
2. **退出时 50% 概率 crash**：线程可能在其他对象析构后还在运行
3. **join 时机混乱**：不知道什么时候 join
4. **难以管理生命周期**：无法控制创建和销毁时机

✅ **正确做法**：

```cpp
// ✅ 正确：thread 作为成员变量
class SingletonWorker {
private:
    std::thread worker_;  // 正确！成员变量
};
```

---

### 🚫 坑 3：detach()

```cpp
// ❌ 错误：使用 detach()
void start() {
    worker_ = std::thread(&SingletonWorker::threadLoop, this);
    worker_.detach();  // 错误！不可控
}
```

**在你这个项目里，detach = 不可控 = 不允许**

**问题：**

1. **线程生命周期不可控**：无法知道线程什么时候退出
2. **资源清理困难**：无法确保资源被正确清理
3. **调试困难**：无法 join，难以追踪线程状态
4. **不符合嵌入式工程实践**：嵌入式应用需要精确控制资源生命周期

✅ **正确做法**：

```cpp
// ✅ 正确：显式 join
void stop() {
    running_.store(false);
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();  // 正确！显式 join
    }
}
```

---

### 🚫 坑 4：while(1) + sleep（busy-loop）

```cpp
// ❌ 错误：busy-loop
void threadLoop() {
    while (running_) {
        doWork();
        usleep(1000);  // 错误！busy-loop
    }
}

// ❌ 错误：轮询检查
void threadLoop() {
    while (running_) {
        if (!tasks_.empty()) {
            processTask();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));  // 错误！
    }
}
```

**这是嵌入式常见慢性病：**

1. **电源抖**：即使没有任务也在消耗 CPU
2. **CPU 抖**：sleep 间隔不确定，CPU 占用不稳定
3. **行为不可预测**：无法精确控制唤醒时机
4. **不符合嵌入式工程实践**：嵌入式应用需要精确控制 CPU 使用

✅ **正确做法**：

```cpp
// ✅ 正确：阻塞等待（condition_variable）
void threadLoop() {
    while (running_) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this]() {
                return !tasks_.empty() || !running_.load();
            });  // 正确！阻塞等待，零 CPU 占用
            
            if (!running_.load()) break;
            
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        
        processTask(task);
    }
}
```

---

### 🚫 坑 5：析构函数里隐式 join

```cpp
// ❌ 错误：在析构函数中 join
~SingletonWorker() {
    if (worker_.joinable()) {
        worker_.join();  // 错误！析构顺序不可控
    }
}
```

**问题：**

1. **析构顺序不可控**：C++ 静态对象析构顺序不确定
2. **可能死锁**：如果线程在等待其他 Singleton，而其他 Singleton 已经析构，会导致死锁
3. **调试极其痛苦**：难以追踪问题
4. **不符合工程实践**：应该在明确的位置控制生命周期

✅ **正确做法**：

```cpp
// ✅ 正确：App 明确 stop()
void app_main_cleanup() {
    SingletonWorker::instance().stop();  // 正确！显式 stop
}

// ✅ 正确：在 stop() 中 join
void stop() {
    running_.store(false);
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();  // 正确！在 stop() 中 join
    }
}
```

---

### 🚫 坑 6：构造函数中做重活

```cpp
// ❌ 错误：构造函数中做耗时操作
SingletonWorker::SingletonWorker() {
    curl_global_init(CURL_GLOBAL_ALL);     // 错误！耗时操作
    connectToServer();                      // 错误！网络操作
    loadConfig();                           // 错误！文件 IO
}
```

**问题：**

1. **初始化阻塞**：构造函数中做耗时操作会导致初始化阻塞
2. **异常处理困难**：构造函数中异常处理复杂
3. **难以测试**：难以模拟和测试
4. **不符合工程实践**：应该在明确的位置初始化资源

✅ **正确做法**：

```cpp
// ✅ 正确：在线程内初始化
void threadLoop() {
    onThreadStart();  // 正确！在线程内初始化
    
    while (running_) {
        // ...
    }
    
    onThreadStop();  // 正确！在线程内清理
}

void onThreadStart() {
    curl_global_init(CURL_GLOBAL_ALL);     // 正确！
    connectToServer();                      // 正确！
    loadConfig();                           // 正确！
}
```

---

### 🚫 坑 7：Singleton 间互相依赖构造

```cpp
// ❌ 错误：A::instance() 里调用 B::instance()
NetworkWorker::NetworkWorker() {
    EventBus::getInstance().subscribe(...);  // 错误！可能导致死锁
}
```

**问题：**

1. **可能导致死锁**：如果 A 和 B 互相依赖，会导致死锁
2. **初始化顺序不确定**：难以控制初始化顺序
3. **难以测试**：难以模拟和测试

✅ **正确做法**：

```cpp
// ✅ 正确：延迟初始化
void NetworkWorker::onThreadStart() {
    EventBus::getInstance().subscribe(...);  // 正确！在线程内初始化
}
```

---

## 五、工程铁律（必须遵守）

> **以下规则必须遵守，违反即视为代码审查不通过。**

1. ✅ **Singleton 线程必须显式 start / stop**
   - 禁止在构造函数中启动线程
   - 禁止在 `instance()` 中隐式启动
   - 必须由 App 主流程显式调用 start() 和 stop()

2. ✅ **禁止在构造函数中启动线程**
   - 构造函数只做轻量初始化
   - 线程在 start() 中创建

3. ✅ **禁止使用 detach**
   - 所有线程必须支持 join
   - 必须在 stop() 中显式 join

4. ✅ **所有线程必须使用阻塞等待**
   - 使用 `condition_variable` 阻塞等待
   - 禁止 busy-loop（while + sleep）
   - 禁止轮询检查

5. ✅ **所有线程必须支持显式 join**
   - 在 stop() 中显式 join
   - 禁止在析构函数中 join
   - 禁止使用 detach

6. ✅ **Singleton 之间不得在构造期互相依赖**
   - 禁止在构造函数中访问其他 Singleton
   - 可以在 onThreadStart() 中访问其他 Singleton

7. ✅ **资源初始化/清理位置明确**
   - 初始化：onThreadStart()（在线程内）
   - 清理：onThreadStop()（在线程内）
   - 禁止在构造函数/析构函数中做重活

---

## 六、适用线程列表

本项目中，以下线程适合直接使用这套模板：

| 线程 | 类名 | 说明 |
|------|------|------|
| **EventDispatch** | `EventDispatchThread` | 事件分发线程 |
| **Network** | `NetworkWorker` | 网络请求处理线程 |
| **Player** | `PlayerAdapter` | 播放器控制线程 |
| **Upgrade** | `UpgradeService` | 升级检测线程 |
| **LogUpload** | `LogUploadService` | 日志上传线程 |

**全部可以直接复用，不用改思想。**

---

## 七、一句话总结

> 在嵌入式 Linux 应用里，**Singleton + std::thread 不是反模式，是对工程风险的主动管理。**
>
> 你现在的方向是：**减少自由度 → 换稳定性 → 换可维护性**
>
> 这是对的，而且是"量产工程"的做法。

---

## 📚 相关文档

- [线程架构基线（最终版）.md](../线程架构基线（最终版）.md)
- [线程Singleton白名单与实现规范.md](./线程Singleton白名单与实现规范.md)
- [标准线程模板与实现指南.md](./标准线程模板与实现指南.md)
- [KTV_App稳定性与自愈设计说明.md](../sdk/KTV_App稳定性与自愈设计说明.md)

---

**最后更新**: 2025-12-30  
**维护者**: 项目团队  
**状态**: ✅ 核心文档（工程规范 - 铁律级）

