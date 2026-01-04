# 事件模型 MVP 实现指南（可落地版）

> **文档版本**：v1.0  
> **最后更新**：2025-12-30  
> **状态**：✅ 核心文档（MVP 可落地版）  
> **适用平台**：F133 / Tina Linux  
> **目标**：提供 MVP 级最小正确的事件模型实现，避免过度设计

---

## 🎯 一句话结论

> **Tina 里的事件模型，可以理解为：onclick → Event → Queue → Service →（必要时）libcurl 发网络请求**

**关键前提**：
- **Event 只负责"发生了什么"**
- **Service 决定"要不要走网络、走不走 libcurl"**
- **libcurl 永远不直接碰 UI / Event**

---

## 📋 目录

1. [最小正确架构（MVP级）](#一-最小正确架构mvp级)
2. [完整C++实现示例](#二-完整c实现示例)
3. [典型使用场景](#三-典型使用场景)
4. [容易踩的坑](#四-容易踩的坑)
5. [命名规范](#五-命名规范)
6. [与现有架构的关系](#六-与现有架构的关系)

---

## 一、最小正确架构（MVP级）

### 架构流程图

```
┌─────────────────────────────────────────────────────────────────┐
│  UI / 输入层（类似 onclick）                                      │
│  • 只做一件事：塞 event 到 queue                                │
│  • 不做：不发网络、不调 libcurl、不写业务逻辑                    │
└─────────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  Event Queue（核心解耦点）                                       │
│  • std::queue + mutex + cond                                    │
│  • 不用 EventBus，不用反射，不用订阅系统                         │
└─────────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  Event Dispatcher（一个线程就够）                                 │
│  • while(1) { pop_event(); dispatch(); }                        │
│  • dispatch 本质是个 switch，只有"路由"，没有业务                │
└─────────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  Service 层（关键点）                                            │
│  • Service 决定要不要转给 libcurl                               │
│  • 业务判断、缓存策略、是否触发网络                              │
└─────────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  NetworkService + libcurl（必须隔离）                            │
│  • libcurl 不直接操作 UI                                         │
│  • libcurl 不直接操作 LVGL                                       │
│  • libcurl 不回调 onclick                                        │
└─────────────────────────────────────────────────────────────────┘
```

---

## 二、完整C++实现示例

### 1️⃣ 事件定义（最小化）

```cpp
// event_types.h
#pragma once

enum class EventType {
    // UI 事件
    EVENT_CATEGORY_CLICK,        // 分类点击
    EVENT_SONG_SELECTED,         // 歌曲选择
    EVENT_SEARCH_SUBMITTED,      // 搜索提交
    EVENT_PLAY_CLICKED,          // 播放按钮点击
    
    // Service 回调事件
    EVENT_CATEGORY_DATA_READY,   // 分类数据就绪
    EVENT_SEARCH_RESULT_READY,   // 搜索结果就绪
    EVENT_PLAY_STARTED,          // 播放开始
    EVENT_PLAY_FINISHED,         // 播放完成
    EVENT_NETWORK_ERROR,         // 网络错误
};

// 最小事件结构（只传必要数据）
struct AppEvent {
    EventType type;
    int arg1 = 0;           // 通用参数1（如 categoryId, songId）
    int arg2 = 0;           // 通用参数2
    void* data = nullptr;   // 可选数据指针（预分配内存）
};

// 注意：不使用 callback / function pointer
// 不使用复杂的数据结构
```

---

### 2️⃣ Event Queue（核心解耦点）

```cpp
// event_queue.h
#pragma once
#include "event_types.h"
#include <queue>
#include <mutex>
#include <condition_variable>

class EventQueue {
public:
    static EventQueue& instance() {
        static EventQueue inst;
        return inst;
    }
    
    // 入队（多生产者，线程安全）
    void enqueue(const AppEvent& ev) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(ev);
        cond_.notify_one();
    }
    
    // 出队（阻塞等待，单消费者）
    bool dequeue(AppEvent& ev, int timeout_ms = -1) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (timeout_ms > 0) {
            if (!cond_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                               [this]() { return !queue_.empty() || !running_; })) {
                return false;  // 超时
            }
        } else {
            cond_.wait(lock, [this]() { return !queue_.empty() || !running_; });
        }
        
        if (!running_ && queue_.empty()) {
            return false;  // 已停止
        }
        
        ev = queue_.front();
        queue_.pop();
        return true;
    }
    
    // 非阻塞出队（UI线程使用）
    bool tryDequeue(AppEvent& ev) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        ev = queue_.front();
        queue_.pop();
        return true;
    }
    
    void stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
        cond_.notify_all();
    }
    
private:
    EventQueue() : running_(true) {}
    ~EventQueue() = default;
    
    std::queue<AppEvent> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::atomic<bool> running_;
};
```

---

### 3️⃣ Event Dispatcher（一个线程就够）

```cpp
// event_dispatcher.h
#pragma once
#include "event_queue.h"
#include "category_service.h"
#include "search_service.h"
#include "player_service.h"
#include <thread>
#include <atomic>

class EventDispatcher {
public:
    static EventDispatcher& instance() {
        static EventDispatcher inst;
        return inst;
    }
    
    void start() {
        if (running_.load()) {
            return;
        }
        running_.store(true);
        thread_ = std::thread(&EventDispatcher::dispatchLoop, this);
    }
    
    void stop() {
        running_.store(false);
        EventQueue::instance().stop();
        if (thread_.joinable()) {
            thread_.join();
        }
    }
    
private:
    EventDispatcher() = default;
    ~EventDispatcher() {
        stop();
    }
    
    // 分发循环（一个线程就够）
    void dispatchLoop() {
        AppEvent ev;
        while (running_.load()) {
            if (EventQueue::instance().dequeue(ev, 100)) {
                dispatch(ev);  // 只有"路由"，没有业务
            }
        }
    }
    
    // 分发函数（本质是个 switch，只有路由）
    void dispatch(const AppEvent& ev) {
        switch (ev.type) {
            case EventType::EVENT_CATEGORY_CLICK:
                CategoryService::instance().onClick(ev.arg1);
                break;
                
            case EventType::EVENT_SEARCH_SUBMITTED:
                SearchService::instance().onSearch(ev.arg1, ev.data);
                break;
                
            case EventType::EVENT_PLAY_CLICKED:
                PlayerService::instance().onPlay(ev.arg1);
                break;
                
            case EventType::EVENT_CATEGORY_DATA_READY:
                // 通过 Presenter 回调更新 UI
                CategoryPresenter::instance().onSvcCategoryDataReady(ev.arg1, ev.data);
                break;
                
            case EventType::EVENT_SEARCH_RESULT_READY:
                SearchPresenter::instance().onSvcSearchResultReady(ev.data);
                break;
                
            case EventType::EVENT_PLAY_STARTED:
                PlayerPresenter::instance().onSvcPlayStarted();
                break;
                
            case EventType::EVENT_NETWORK_ERROR:
                // 统一错误处理
                ErrorHandler::instance().onNetworkError(ev.arg1);
                break;
                
            default:
                // 未知事件，记录日志
                syslog(LOG_WARNING, "[ktv][event] Unknown event type: %d", static_cast<int>(ev.type));
                break;
        }
    }
    
    std::thread thread_;
    std::atomic<bool> running_{false};
};
```

---

### 4️⃣ Service 层（关键点：决定是否走网络）

```cpp
// category_service.h
#pragma once
#include "event_queue.h"
#include "network_service.h"

class CategoryService {
public:
    static CategoryService& instance() {
        static CategoryService inst;
        return inst;
    }
    
    // Service 决定要不要转给 libcurl
    void onClick(int categoryId) {
        // 1. 业务判断：检查缓存
        if (cache_hit(categoryId)) {
            // 缓存命中，直接更新 UI
            AppEvent ev;
            ev.type = EventType::EVENT_CATEGORY_DATA_READY;
            ev.arg1 = categoryId;
            ev.data = getCachedData(categoryId);
            EventQueue::instance().enqueue(ev);
        } else {
            // 缓存未命中，触发网络请求
            NetworkService::instance().fetchCategory(categoryId);
        }
    }
    
private:
    bool cache_hit(int categoryId) {
        // 检查缓存逻辑
        return false;  // 示例
    }
    
    void* getCachedData(int categoryId) {
        // 获取缓存数据
        return nullptr;  // 示例
    }
};
```

---

### 5️⃣ NetworkService + libcurl（必须隔离）

```cpp
// network_service.h
#pragma once
#include "event_queue.h"
#include <curl/curl.h>
#include <string>

class NetworkService {
public:
    static NetworkService& instance() {
        static NetworkService inst;
        return inst;
    }
    
    void fetchCategory(int categoryId) {
        // libcurl 请求
        std::string url = "http://api.example.com/category/" + std::to_string(categoryId);
        
        // 在 Network Worker 线程中执行
        NetworkWorker::instance().post([url, categoryId]() {
            CURL* curl = curl_easy_init();
            if (!curl) {
                // 错误处理
                AppEvent ev;
                ev.type = EventType::EVENT_NETWORK_ERROR;
                ev.arg1 = -1;
                EventQueue::instance().enqueue(ev);
                return;
            }
            
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
            
            std::string response;
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            
            CURLcode res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            
            if (res == CURLE_OK) {
                // 成功：解析 JSON，发送事件
                AppEvent ev;
                ev.type = EventType::EVENT_CATEGORY_DATA_READY;
                ev.arg1 = categoryId;
                ev.data = parseJson(response);  // 预分配内存
                EventQueue::instance().enqueue(ev);
            } else {
                // 失败：发送错误事件
                AppEvent ev;
                ev.type = EventType::EVENT_NETWORK_ERROR;
                ev.arg1 = res;
                EventQueue::instance().enqueue(ev);
            }
        });
    }
    
private:
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
        data->append((char*)contents, size * nmemb);
        return size * nmemb;
    }
    
    void* parseJson(const std::string& json) {
        // JSON 解析（使用预分配内存）
        // 返回数据指针
        return nullptr;  // 示例
    }
};
```

---

### 6️⃣ UI 层（只塞 event 到 queue）

```cpp
// category_page.h
#pragma once
#include "event_queue.h"

class CategoryPage {
public:
    // UI 回调（类似 onclick）
    static void onCategoryClick(lv_event_t* e) {
        int categoryId = getCategoryIdFromEvent(e);
        
        // 只做一件事：塞 event 到 queue
        AppEvent ev;
        ev.type = EventType::EVENT_CATEGORY_CLICK;
        ev.arg1 = categoryId;
        EventQueue::instance().enqueue(ev);
        
        // 不做：不发网络、不调 libcurl、不写业务逻辑
    }
    
    // UI 更新（通过 Presenter 回调）
    void onSvcCategoryDataReady(int categoryId, void* data) {
        // 更新 UI（在 UI 线程中）
        renderCategoryList(data);
    }
    
private:
    void renderCategoryList(void* data) {
        // 渲染分类列表
    }
};
```

---

## 三、典型使用场景

### 场景1：用户点击分类

```
用户点击分类按钮
    │
    ▼
CategoryPage::onCategoryClick()
    │
    ▼
EventQueue::enqueue(EVENT_CATEGORY_CLICK)
    │
    ▼
EventDispatcher::dispatch()
    │
    ▼
CategoryService::onClick(categoryId)
    │
    ├─► 缓存命中？
    │   │
    │   ├─► 是：直接发送 EVENT_CATEGORY_DATA_READY
    │   │
    │   └─► 否：NetworkService::fetchCategory()
    │           │
    │           ▼
    │       libcurl 请求
    │           │
    │           ▼
    │       发送 EVENT_CATEGORY_DATA_READY 或 EVENT_NETWORK_ERROR
    │
    ▼
EventDispatcher::dispatch()
    │
    ▼
CategoryPresenter::onSvcCategoryDataReady()
    │
    ▼
CategoryPage::renderCategoryList()
```

### 场景2：用户搜索歌曲

```
用户输入搜索关键词
    │
    ▼
SearchPage::onSearchSubmitted()
    │
    ▼
EventQueue::enqueue(EVENT_SEARCH_SUBMITTED)
    │
    ▼
EventDispatcher::dispatch()
    │
    ▼
SearchService::onSearch()
    │
    ▼
NetworkService::fetchSearch()
    │
    ▼
libcurl 请求
    │
    ▼
发送 EVENT_SEARCH_RESULT_READY 或 EVENT_NETWORK_ERROR
    │
    ▼
SearchPresenter::onSvcSearchResultReady()
    │
    ▼
SearchPage::renderSearchResult()
```

---

## 四、容易踩的坑

### ❌ 坑1：在 UI 线程里直接 libcurl

```cpp
// ❌ 错误：在 UI 线程里直接 libcurl
void CategoryPage::onCategoryClick(lv_event_t* e) {
    CURL* curl = curl_easy_init();  // 错误！阻塞 UI
    curl_easy_perform(curl);        // 错误！卡顿
    curl_easy_cleanup(curl);
}

// ✅ 正确：只塞 event 到 queue
void CategoryPage::onCategoryClick(lv_event_t* e) {
    AppEvent ev;
    ev.type = EventType::EVENT_CATEGORY_CLICK;
    ev.arg1 = getCategoryId(e);
    EventQueue::instance().enqueue(ev);
}
```

### ❌ 坑2：Event 里塞 callback / function pointer

```cpp
// ❌ 错误：Event 里塞 callback
struct AppEvent {
    EventType type;
    std::function<void()> callback;  // 错误！可读性爆炸
};

// ✅ 正确：Event 只传数据
struct AppEvent {
    EventType type;
    int arg1 = 0;
    int arg2 = 0;
    void* data = nullptr;  // 预分配内存
};
```

### ❌ 坑3：Event = Service（所有业务写在 dispatch 里）

```cpp
// ❌ 错误：所有业务写在 dispatch 里
void EventDispatcher::dispatch(const AppEvent& ev) {
    switch (ev.type) {
        case EventType::EVENT_CATEGORY_CLICK:
            // 错误！业务逻辑不应该在这里
            CURL* curl = curl_easy_init();
            // ... 1000 行业务代码
            break;
    }
}

// ✅ 正确：dispatch 只有路由，业务在 Service
void EventDispatcher::dispatch(const AppEvent& ev) {
    switch (ev.type) {
        case EventType::EVENT_CATEGORY_CLICK:
            CategoryService::instance().onClick(ev.arg1);  // 只有路由
            break;
    }
}
```

---

## 五、命名规范

### 推荐命名（企业级但不重）

| 层级 | 命名规范 | 示例 |
|------|---------|------|
| **UI** | `{Page}Page` | `CategoryPage`, `PlayerPage` |
| **Event** | `EVENT_{动作}` | `EVENT_CATEGORY_CLICK`, `EVENT_PLAY_CLICKED` |
| **Dispatcher** | `EventDispatcher` | `EventDispatcher::instance()` |
| **Service** | `{Module}Service` | `CategoryService`, `PlayerService` |
| **Network** | `NetworkService` | `NetworkService::instance()` |

**看到名字就知道在哪一层，不需要读代码。**

---

## 六、与现有架构的关系

### 与命名规范的关系

本事件模型与 [应用层命名规范（架构约束版）.md](./应用层命名规范（架构约束版）.md) 完全一致：

- **UI 层**：`emitXxx()` → 塞 event 到 queue
- **Presenter 层**：`onUiXxx()` → 接收 UI 事件，`onSvcXxx()` → 接收 Service 回调
- **Service 层**：`requestXxx()` → 决定是否走网络
- **Network 层**：`fetchXxx()` → libcurl 请求

### 与线程架构的关系

本事件模型与 [线程架构基线（最终版）.md](../线程架构基线（最终版）.md) 完全一致：

- **Event Dispatcher**：运行在 Event Loop 线程
- **NetworkService**：运行在 Network Worker 线程
- **UI 更新**：通过 UiEventQueue 回到 UI 线程

### 与消息队列的关系

本事件模型使用 `std::queue + std::mutex + condition_variable`，与 [消息队列完整指南.md](../消息队列完整指南.md) 的技术决策完全一致。

---

## 📚 相关文档

- [应用层命名规范（架构约束版）.md](./应用层命名规范（架构约束版）.md) ⭐⭐⭐ **必读**
- [线程架构基线（最终版）.md](../线程架构基线（最终版）.md) ⭐⭐⭐ **必读**
- [消息队列完整指南.md](../消息队列完整指南.md) ⭐⭐⭐ **必读**
- [事件架构规范.md](../architecture/事件架构规范.md) ⭐⭐ **参考**

---

## 💡 总结

### 核心原则

1. **Event 只负责"发生了什么"**：不包含业务逻辑，不包含 callback
2. **Service 决定"要不要走网络"**：业务判断、缓存策略都在 Service 层
3. **libcurl 永远不直接碰 UI / Event**：必须通过 NetworkService 隔离

### MVP 级架构

- **UI 层**：只塞 event 到 queue
- **Event Queue**：`std::queue + mutex + cond`
- **Event Dispatcher**：一个线程，switch 路由
- **Service 层**：决定是否走网络
- **NetworkService**：libcurl 隔离层

### 避免过度设计

- ❌ 不用 EventBus
- ❌ 不用反射
- ❌ 不用订阅系统
- ❌ 不用 Rx
- ❌ 不用状态机框架

**Event + Queue + Service + libcurl 已经是最优复杂度解。**

---

**最后更新**: 2025-12-30  
**维护者**: 项目团队  
**状态**: ✅ 核心文档（MVP 可落地版）

