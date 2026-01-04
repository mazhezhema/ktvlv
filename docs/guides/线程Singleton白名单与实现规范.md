# 线程 Singleton 白名单与实现规范（F133 / Tina Linux）

> **文档版本**：v1.1  
> **最后更新**：2025-12-30  
> **状态**：✅ 已合并到编码规范（保留作为历史参考）  
> **适用平台**：F133 / Tina Linux  
> **目标**：明确哪些线程必须使用 Singleton，以及如何正确实现

> **⚠️ 重要提示**：本文档内容已合并到 [KTV_App线程Singleton编码规范（最终版）.md](./KTV_App线程Singleton编码规范（最终版）.md)，建议直接参考编码规范文档。本文档保留作为历史参考。

---

## 🎯 核心原则（一句话）

> **线程本身不是 Singleton，线程"宿主对象"是 Singleton。**

> **📌 推荐阅读**：[KTV_App线程Singleton编码规范（最终版）.md](./KTV_App线程Singleton编码规范（最终版）.md) ⭐⭐⭐ **必读（铁律级）** - 包含完整的模板代码、避坑指南和工程铁律

---

## 📋 目录

1. [Singleton 线程白名单](#一-singleton-线程白名单)
2. [标准实现模式](#二-标准实现模式)
3. [防炸规则（必须遵守）](#三-防炸规则必须遵守)
4. [禁止使用的场景](#四-禁止使用的场景)
5. [常见问题与解答](#五-常见问题与解答)

---

## 一、Singleton 线程白名单

### ✅ 必须使用 Singleton 的线程

本项目中，以下核心线程的宿主对象**必须**是 Singleton：

| 线程 | 宿主对象 | Singleton 名称 | 原因 | 生命周期 | 状态 |
|------|---------|---------------|------|---------|------|
| **UI/LVGL 主线程** | `UISystem` | `UISystem::instance()` | 全局唯一 display、input，不允许多实例 | App 生命周期 | ✅ 必须 |
| **Event Dispatch** | `EventBus` | `EventBus::getInstance()` | 全局事件总线，所有模块共享 | App 生命周期 | ✅ 必须 |
| **Network Worker** | `NetworkWorker` | `NetworkWorker::instance()` | libcurl 全局初始化、连接复用、DNS/TLS 状态共享 | App 生命周期 | ✅ 必须 |
| **Player Worker** | `PlayerAdapter` | `PlayerAdapter::instance()` | TPlayer 全局唯一、硬件资源独占 | App 生命周期 | ✅ 必须 |
| **LogUpload** | `LogUploadService` | `LogUploadService::instance()` | 全局日志上传服务 | App 生命周期 | ✅ 必须 |
| **Upgrade Checker** | `UpgradeService` | `UpgradeService::instance()` | 全局升级检测，不允许重复执行 | App 生命周期 | ✅ 必须 |

### 详细说明

#### 1. UI/LVGL 主线程

**原因**：
- LVGL 的 display 和 input 驱动全局唯一
- 不允许多实例，否则会导致资源冲突
- UI 线程是应用的主控制线程

**实现要点**：
```cpp
class UISystem {
public:
    static UISystem& instance() {
        static UISystem inst;
        return inst;
    }
    
    void init();
    void run();  // 主循环
    void cleanup();
};
```

#### 2. Event Dispatch 线程

**原因**：
- 全局事件总线，所有模块都需要访问
- 事件分发逻辑应该集中管理
- 避免事件丢失或重复处理

**实现要点**：
```cpp
class EventBus {
public:
    static EventBus& getInstance() {
        static EventBus inst;
        return inst;
    }
    
    void publish(const Event& ev);
    void dispatchOnUiThread();
};
```

#### 3. Network Worker

**原因**：
- libcurl 全局初始化（`curl_global_init()`）
- HTTP 连接复用（连接池）
- DNS 缓存、TLS 状态共享
- 多个网络请求需要共享底层资源

**实现要点**：
```cpp
class NetworkWorker {
public:
    static NetworkWorker& instance() {
        static NetworkWorker inst;
        return inst;
    }
    
    void postHttpRequest(const HttpRequest& req);
    void postWebSocketMessage(const std::string& msg);
};
```

#### 4. Player Worker

**原因**：
- TPlayer SDK 通常全局唯一
- 硬件资源独占（音频输出、视频输出）
- 播放器状态需要全局管理
- 多个播放请求需要串行化处理

**实现要点**：
```cpp
class PlayerAdapter {
public:
    static PlayerAdapter& instance() {
        static PlayerAdapter inst;
        return inst;
    }
    
    void play(const std::string& url);
    void pause();
    void stop();
};
```

#### 5. LogUpload

**原因**：
- 全局日志上传服务
- 日志缓冲区需要集中管理
- 上传逻辑应该统一处理

#### 6. Upgrade Checker

**原因**：
- 升级检测是全局行为
- 不允许重复执行升级检测
- 升级状态需要全局管理

---

## 二、标准实现模式

### 2.1 完整的 Singleton 线程模板

```cpp
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <syslog.h>

/**
 * @brief 标准 Singleton 线程模式
 * 
 * 特点：
 * - 构造私有，析构不暴露
 * - 不允许 copy / move
 * - 线程在构造函数中启动（可选：或通过 start() 启动）
 * - App 退出时显式调用 stop()
 */
class NetworkWorker {
public:
    struct Task {
        int type;
        void* data;
        // ... 任务数据
    };

    // Singleton 访问点
    static NetworkWorker& instance() {
        static NetworkWorker inst;
        return inst;
    }

    // 禁止拷贝和移动
    NetworkWorker(const NetworkWorker&) = delete;
    NetworkWorker& operator=(const NetworkWorker&) = delete;
    NetworkWorker(NetworkWorker&&) = delete;
    NetworkWorker& operator=(NetworkWorker&&) = delete;

    // 公共接口：投递任务
    void post(Task task) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(task));
        }
        condition_.notify_one();
    }

    // 公共接口：停止线程（必须显式调用）
    void stop() {
        if (!running_.exchange(false)) {
            return;  // 已经停止
        }
        
        condition_.notify_all();  // 唤醒等待的线程
        
        if (thread_.joinable()) {
            thread_.join();  // 等待线程退出
        }
        
        syslog(LOG_INFO, "[ktv][thread] NetworkWorker stopped");
    }

    // 可选：启动方法（如果不在构造函数中启动）
    void start() {
        if (running_.exchange(true)) {
            return;  // 已经启动
        }
        thread_ = std::thread(&NetworkWorker::workerLoop, this);
        syslog(LOG_INFO, "[ktv][thread] NetworkWorker started");
    }

private:
    // 私有构造函数
    NetworkWorker() : running_(false) {
        // 方式1：在构造函数中启动（简单场景）
        // running_.store(true);
        // thread_ = std::thread(&NetworkWorker::workerLoop, this);
        
        // 方式2：延迟启动（推荐，避免初始化顺序问题）
        // 通过 start() 方法启动
    }

    // 私有析构函数（不允许自动析构）
    ~NetworkWorker() {
        // 不允许自动析构，必须显式调用 stop()
        // 如果忘记调用 stop()，这里应该记录错误日志
        if (running_.load()) {
            syslog(LOG_ERR, "[ktv][thread] NetworkWorker destroyed without stop()!");
            // 紧急清理（不推荐，但总比崩溃好）
            stop();
        }
    }

    // 工作循环
    void workerLoop() {
        syslog(LOG_INFO, "[ktv][thread] NetworkWorker loop started");
        
        // 在线程 loop 内初始化（避免构造函数中做重活）
        initNetwork();
        
        while (running_.load()) {
            Task task;
            
            // 阻塞等待任务
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this]() {
                    return !queue_.empty() || !running_.load();
                });
                
                if (!running_.load()) {
                    break;
                }
                
                if (queue_.empty()) {
                    continue;
                }
                
                task = queue_.front();
                queue_.pop();
            }
            
            // 处理任务（在锁外执行）
            processTask(task);
        }
        
        cleanupNetwork();
        syslog(LOG_INFO, "[ktv][thread] NetworkWorker loop exited");
    }

    void processTask(const Task& task) {
        // 具体任务处理逻辑
        syslog(LOG_DEBUG, "[ktv][thread] Processing task type=%d", task.type);
    }

    void initNetwork() {
        // 网络初始化（在线程内执行，避免构造函数中做重活）
        // curl_global_init(CURL_GLOBAL_ALL);
    }

    void cleanupNetwork() {
        // 网络清理
        // curl_global_cleanup();
    }

    std::thread thread_;
    std::atomic<bool> running_;
    std::queue<Task> queue_;
    std::mutex mutex_;
    std::condition_variable condition_;
};
```

### 2.2 使用示例

```cpp
// App 启动时初始化
void app_main_init() {
    // 方式1：构造函数中启动
    NetworkWorker::instance();  // 自动启动
    
    // 方式2：延迟启动（推荐）
    NetworkWorker::instance().start();
}

// 业务代码中使用
void sendHttpRequest(const std::string& url) {
    NetworkWorker::Task task;
    task.type = TASK_HTTP_REQUEST;
    task.data = /* ... */;
    NetworkWorker::instance().post(std::move(task));
}

// App 退出时清理
void app_main_cleanup() {
    NetworkWorker::instance().stop();  // 显式停止
}
```

---

## 三、防炸规则（必须遵守）

### 🚫 规则 1：禁止在构造函数里做重活

**问题**：
- 构造函数中做耗时操作会导致初始化阻塞
- 如果多个 Singleton 在构造函数中互相依赖，可能导致死锁
- 构造函数异常处理复杂

**错误示例**：
```cpp
// ❌ 错误：构造函数中做耗时操作
NetworkWorker::NetworkWorker() {
    curl_global_init(CURL_GLOBAL_ALL);     // 错误！耗时操作
    connectToServer();                      // 错误！网络操作
    loadConfig();                           // 错误！文件 IO
    
    thread_ = std::thread(&NetworkWorker::workerLoop, this);
}
```

**正确做法**：
```cpp
// ✅ 正确：只启动线程，初始化放在线程 loop 内
NetworkWorker::NetworkWorker() : running_(false) {
    // 只做必要的轻量级初始化
    // 不启动线程，通过 start() 启动
}

void NetworkWorker::start() {
    running_.store(true);
    thread_ = std::thread(&NetworkWorker::workerLoop, this);
}

void NetworkWorker::workerLoop() {
    // 在线程 loop 内初始化
    curl_global_init(CURL_GLOBAL_ALL);     // 正确！
    connectToServer();                      // 正确！
    loadConfig();                           // 正确！
    
    while (running_.load()) {
        // 处理任务
    }
    
    // 清理
    curl_global_cleanup();
}
```

### 🚫 规则 2：禁止 Singleton 间互相依赖构造

**问题**：
- 如果 A::instance() 在构造函数中调用 B::instance()，可能导致死锁
- 初始化顺序不确定，可能导致未初始化访问
- 难以调试和测试

**错误示例**：
```cpp
// ❌ 错误：A::instance() 里调用 B::instance()
NetworkWorker::NetworkWorker() {
    EventBus::getInstance().subscribe(...);  // 错误！可能导致死锁
    thread_ = std::thread(&NetworkWorker::workerLoop, this);
}

PlayerAdapter::PlayerAdapter() {
    NetworkWorker::instance().post(...);     // 错误！可能导致死锁
    thread_ = std::thread(&PlayerAdapter::workerLoop, this);
}
```

**正确做法**：
```cpp
// ✅ 正确：延迟初始化或通过 start() 方法初始化
NetworkWorker::NetworkWorker() : running_(false) {
    // 不在这里初始化依赖
}

void NetworkWorker::start() {
    running_.store(true);
    thread_ = std::thread(&NetworkWorker::workerLoop, this);
    
    // 在 start() 中初始化依赖（此时所有 Singleton 都已创建）
    EventBus::getInstance().subscribe(...);  // 正确！
}

// 或者：在 workerLoop() 中初始化依赖
void NetworkWorker::workerLoop() {
    // 在线程内初始化依赖（更安全）
    EventBus::getInstance().subscribe(...);  // 正确！
    
    while (running_.load()) {
        // 处理任务
    }
}
```

### 🚫 规则 3：禁止静态析构顺序依赖

**问题**：
- C++ 静态对象的析构顺序不确定
- 如果 Singleton A 在析构时访问 Singleton B，但 B 已经析构，会导致未定义行为
- 难以控制和调试

**错误示例**：
```cpp
// ❌ 错误：依赖 C++ static 析构顺序
// 全局变量或静态变量中持有 Singleton 引用
static NetworkWorker& g_worker = NetworkWorker::instance();  // 危险！

class SomeClass {
    static NetworkWorker& worker_;  // 危险！
};
NetworkWorker& SomeClass::worker_ = NetworkWorker::instance();  // 危险！

// ❌ 错误：在析构函数中访问其他 Singleton
NetworkWorker::~NetworkWorker() {
    EventBus::getInstance().unsubscribe(...);  // 错误！EventBus 可能已析构
}
```

**正确做法**：
```cpp
// ✅ 正确：App 主退出时显式 stop()
void app_main_cleanup() {
    // 按依赖顺序停止（依赖者先停止）
    NetworkWorker::instance().stop();      // 先停止依赖者
    PlayerAdapter::instance().stop();
    EventBus::getInstance().stop();        // 最后停止被依赖者
    UISystem::instance().cleanup();
}

// ✅ 正确：在 stop() 中清理依赖（此时所有 Singleton 都还存在）
void NetworkWorker::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    
    // 在 stop() 中清理依赖（安全，因为所有 Singleton 都还存在）
    EventBus::getInstance().unsubscribe(...);  // 正确！
    
    condition_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

// ✅ 正确：使用时直接调用 instance()，不存储引用
void someFunction() {
    NetworkWorker::instance().post(task);  // 正确！每次都获取引用
}
```

---

## 四、禁止使用的场景

### ❌ 页面级线程（不应该创建线程）

以下场景**不应该**创建线程，更不应该使用 Singleton：

| 场景 | 错误做法 | 正确做法 |
|------|---------|---------|
| **搜索一次** | 创建 `SearchThread` | 投递到 `NetworkWorker::instance().post()` |
| **列表刷新一次** | 创建 `RefreshThread` | 投递到 `NetworkWorker::instance().post()` |
| **点歌一次** | 创建 `PlayThread` | 投递到 `PlayerAdapter::instance().play()` |
| **HTTP 请求** | 创建 `HttpThread` | 投递到 `NetworkWorker::instance().post()` |
| **文件 IO** | 创建 `FileIOThread` | 投递到 `CacheWorker::instance().post()` |
| **JSON 解析** | 创建 `ParseThread` | 投递到工作线程队列 |

### 错误示例

```cpp
// ❌ 错误：为每次操作创建线程
void searchSongs(const std::string& keyword) {
    std::thread([keyword]() {
        // 搜索逻辑
        auto results = httpGet("/api/search?q=" + keyword);
        // ...
    }).detach();  // 错误！线程创建成本高，可能导致资源泄漏
}

// ✅ 正确：投递到已有 Worker 队列
void searchSongs(const std::string& keyword) {
    NetworkWorker::Task task;
    task.type = TASK_SEARCH;
    task.keyword = keyword;
    NetworkWorker::instance().post(std::move(task));  // 正确！
}
```

---

## 五、常见问题与解答

### Q1: 为什么线程宿主对象要用 Singleton？

**A**: 在嵌入式 Linux App 中，核心线程（UI、Event、Network、Player）对应全局唯一资源（LVGL display、事件总线、libcurl、TPlayer），这些资源天然需要全局唯一。使用 Singleton 可以：
- 简化生命周期管理（App 启动创建，App 退出销毁）
- 避免多实例导致资源冲突
- 防止工程师乱 new 线程

### Q2: 所有线程都要用 Singleton 吗？

**A**: **不是**。只有核心线程（UI、Event、Network、Player、LogUpload、Upgrade）需要使用 Singleton。业务逻辑不应该创建线程，应该投递任务到已有的 Worker 队列。

### Q3: Singleton 线程什么时候启动？

**A**: 推荐在 App 启动时显式调用 `start()` 方法启动，而不是在构造函数中启动。这样可以：
- 避免初始化顺序问题
- 避免 Singleton 间互相依赖构造
- 更清晰的初始化流程

### Q4: Singleton 线程什么时候停止？

**A**: **必须**在 App 退出时显式调用 `stop()` 方法停止，不能依赖 C++ 静态析构顺序。推荐在 `app_main_cleanup()` 中按依赖顺序停止。

### Q5: 如何避免 Singleton 初始化顺序问题？

**A**: 
1. 构造函数中不做重活，只做轻量级初始化
2. 通过 `start()` 方法延迟启动线程
3. 在 `start()` 或 `workerLoop()` 中初始化依赖
4. 避免 Singleton 间互相依赖构造

### Q6: 为什么不允许在构造函数中做重活？

**A**: 
- 构造函数中做耗时操作会导致初始化阻塞
- 如果多个 Singleton 在构造函数中互相依赖，可能导致死锁
- 构造函数异常处理复杂
- 难以测试和调试

---

## 📚 相关文档

- [线程架构基线（最终版）.md](../线程架构基线（最终版）.md)
- [标准线程模板与实现指南.md](./标准线程模板与实现指南.md)
- [资源管理规范v1.md](../资源管理规范v1.md)
- [KTV_App稳定性与自愈设计说明.md](../sdk/KTV_App稳定性与自愈设计说明.md)

---

**最后更新**: 2025-12-30  
**维护者**: 项目团队

