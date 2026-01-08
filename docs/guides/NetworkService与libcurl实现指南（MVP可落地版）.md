# NetworkService 与 libcurl 实现指南（MVP 可落地版）

> **文档版本**：v1.0  
> **最后更新**：2025-12-30  
> **状态**：✅ 核心文档（MVP 可落地版）  
> **适用平台**：F133 / Tina Linux  
> **目标**：提供 NetworkService + libcurl + Event 的完整实现指南，避免回调地狱

---

## 🎯 一句话结论

> **libcurl：是的，全局一个（singleton / 单 worker）  
> 返回值：绝对不要靠回调直接打 Service / UI  
> 正解：网络线程 → push event → Service 收结果 → UI 刷新**

---

## 📋 目录

1. [核心原则](#一-核心原则)
2. [架构设计](#二-架构设计)
3. [完整C++实现示例](#三-完整c实现示例)
4. [数据流详解](#四-数据流详解)
5. [libcurl 回调的正确用法](#五-libcurl-回调的正确用法)
6. [禁止事项（硬规则）](#六-禁止事项硬规则)
7. [线程模型与类图](#七-线程模型与类图)

---

## 一、核心原则

### 1.1 libcurl 要不要全局只有一个？

**✅ 推荐答案：逻辑上一个 NetworkService，libcurl 只在这里用**

不是"全局到处用的 libcurl"，而是：

```
NetworkService（singleton）
 └── libcurl（只在这个类里）
```

### 为什么必须这么干？

**现实原因**：
- F133 / Tina 平台
- 两个工程师
- MVP 阶段
- 不碰底层调度

👉 **你根本不具备管理多个 curl handle + 多线程网络的成本**

### 推荐形态（MVP 稳妥型）

| 模块 | 决策 |
|------|------|
| **libcurl** | 只在 NetworkService，用 singleton |
| **网络线程** | 1 个 |
| **并发** | 不要 |
| **返回结果** | Event |
| **业务处理** | Service |
| **UI 刷新** | UI 线程 |
| **回调** | 仅限 libcurl 内部 |

---

## 二、架构设计

### 2.1 完整数据流

```
┌─────────────────────────────────────────────────────────────────┐
│  Service 层（发起请求）                                          │
│  CategoryService::onClick(id)                                   │
│    ↓                                                             │
│  NetworkService::instance().fetchCategory(id)                    │
└─────────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  NetworkService（网络线程）                                      │
│  • curl_easy_perform()                                          │
│  • libcurl 回调只负责收数据（write callback）                    │
│  • 解析 JSON（或留给 Service）                                   │
│  • push_event(EVENT_CATEGORY_DATA_READY, id, payload_ptr)       │
└─────────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  EventQueue（事件队列）                                          │
│  • 网络线程写入事件                                               │
│  • EventDispatcher 消费事件                                     │
└─────────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  EventDispatcher（事件分发线程）                                  │
│  case EVENT_CATEGORY_DATA_READY:                                │
│    CategoryService::onDataReady(ev.data);                       │
└─────────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  Service 层（处理结果）                                          │
│  CategoryService::onDataReady(Data* data)                       │
│    • cache_save(data)                                           │
│    • 决定 UI 行为                                                │
└─────────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  UI 线程（刷新界面）                                              │
│  UIService::refreshCategory(Data* data)                          │
│    • lvgl safe call                                             │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 关键设计点

1. **libcurl 回调只负责收数据**：不包含业务逻辑
2. **数据处理完 → push 一个 EVENT**：通过事件驱动业务
3. **Service 在事件线程里接**：决定怎么刷新 UI
4. **UI 刷新一定在 UI 线程**：保证线程安全

---

## 三、完整C++实现示例

### 3.1 NetworkService 类（Singleton）

```cpp
// network_service.h
#pragma once
#include <string>
#include <curl/curl.h>
#include "event_queue.h"
#include "event_types.h"
#include <memory>
#include <atomic>

// 响应数据结构（预分配内存）
struct HttpResponse {
    std::string body;      // 响应体
    int status_code = 0;   // HTTP 状态码
    int curl_code = 0;     // libcurl 错误码
    bool success = false;  // 是否成功
};

class NetworkService {
public:
    static NetworkService& instance() {
        static NetworkService inst;
        return inst;
    }
    
    // 初始化（在 Network Worker 线程启动时调用）
    bool init();
    
    // 清理（在 Network Worker 线程停止时调用）
    void cleanup();
    
    // 发起 HTTP GET 请求
    void fetchCategory(int categoryId);
    void fetchSearch(const std::string& keyword);
    void fetchSongList(int page, int size);
    
    // 发起 HTTP POST 请求
    void postQueueAdd(int songId);
    void postLogin(const std::string& username, const std::string& password);
    
private:
    NetworkService() = default;
    ~NetworkService() = default;
    NetworkService(const NetworkService&) = delete;
    NetworkService& operator=(const NetworkService&) = delete;
    
    // 执行 HTTP 请求（内部方法）
    HttpResponse performRequest(const std::string& url, const std::string& method = "GET", 
                                 const std::string& postData = "");
    
    // libcurl 回调函数（只负责收数据）
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userdata);
    static size_t headerCallback(void* contents, size_t size, size_t nmemb, void* userdata);
    
    // 发送事件到队列
    void sendEvent(EventType type, int arg1, void* data = nullptr);
    void sendNetworkError(int curlCode, const std::string& url);
    
private:
    CURL* curl_ = nullptr;              // libcurl handle（只在这里）
    std::atomic<bool> initialized_{false};
    
    // 请求缓冲区（预分配）
    static constexpr size_t MAX_RESPONSE_SIZE = 64 * 1024;  // 64KB
    char response_buffer_[MAX_RESPONSE_SIZE];
    size_t response_size_ = 0;
};
```

### 3.2 NetworkService 实现

```cpp
// network_service.cpp
#include "network_service.h"
#include "event_queue.h"
#include <syslog.h>
#include <cstring>

bool NetworkService::init() {
    if (initialized_.load()) {
        return true;
    }
    
    // curl_global_init 只调用一次（在 Network Worker 线程启动时）
    CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
    if (res != CURLE_OK) {
        syslog(LOG_ERR, "[ktv][network] curl_global_init failed: %s", curl_easy_strerror(res));
        return false;
    }
    
    curl_ = curl_easy_init();
    if (!curl_) {
        syslog(LOG_ERR, "[ktv][network] curl_easy_init failed");
        curl_global_cleanup();
        return false;
    }
    
    // 设置默认选项
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 10L);              // 10秒超时
    curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, 5L);        // 5秒连接超时
    curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);       // 跟随重定向
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, headerCallback);
    
    initialized_.store(true);
    syslog(LOG_INFO, "[ktv][network] NetworkService initialized");
    return true;
}

void NetworkService::cleanup() {
    if (!initialized_.load()) {
        return;
    }
    
    if (curl_) {
        curl_easy_cleanup(curl_);
        curl_ = nullptr;
    }
    
    curl_global_cleanup();
    initialized_.store(false);
    syslog(LOG_INFO, "[ktv][network] NetworkService cleaned up");
}

void NetworkService::fetchCategory(int categoryId) {
    std::string url = "http://api.example.com/category/" + std::to_string(categoryId);
    
    // 在 Network Worker 线程中执行
    NetworkWorker::instance().post([this, url, categoryId]() {
        HttpResponse response = performRequest(url);
        
        if (response.success) {
            // 成功：解析 JSON，发送事件
            // 注意：这里可以解析 JSON，也可以留给 Service 解析
            void* data = parseCategoryJson(response.body);  // 预分配内存
            sendEvent(EventType::EVENT_CATEGORY_DATA_READY, categoryId, data);
        } else {
            // 失败：发送错误事件
            sendNetworkError(response.curl_code, url);
        }
    });
}

void NetworkService::fetchSearch(const std::string& keyword) {
    std::string url = "http://api.example.com/search?q=" + encodeUrl(keyword);
    
    NetworkWorker::instance().post([this, url, keyword]() {
        HttpResponse response = performRequest(url);
        
        if (response.success) {
            void* data = parseSearchJson(response.body);  // 预分配内存
            sendEvent(EventType::EVENT_SEARCH_RESULT_READY, 0, data);
        } else {
            sendNetworkError(response.curl_code, url);
        }
    });
}

HttpResponse NetworkService::performRequest(const std::string& url, 
                                            const std::string& method,
                                            const std::string& postData) {
    HttpResponse response;
    
    if (!curl_) {
        syslog(LOG_ERR, "[ktv][network] curl handle not initialized");
        response.curl_code = CURLE_FAILED_INIT;
        return response;
    }
    
    // 重置缓冲区
    response_size_ = 0;
    memset(response_buffer_, 0, sizeof(response_buffer_));
    
    // 设置 URL
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    
    // 设置回调数据指针
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(curl_, CURLOPT_HEADERDATA, this);
    
    // 设置请求方法
    if (method == "POST") {
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, postData.c_str());
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, postData.length());
    } else {
        curl_easy_setopt(curl_, CURLOPT_HTTPGET, 1L);
    }
    
    // 执行请求
    CURLcode res = curl_easy_perform(curl_);
    
    if (res == CURLE_OK) {
        // 获取 HTTP 状态码
        long http_code = 0;
        curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &http_code);
        
        response.status_code = static_cast<int>(http_code);
        response.body.assign(response_buffer_, response_size_);
        response.success = (http_code >= 200 && http_code < 300);
        response.curl_code = CURLE_OK;
        
        syslog(LOG_INFO, "[ktv][network] Request success: %s, status: %d, size: %zu", 
               url.c_str(), response.status_code, response_size_);
    } else {
        response.curl_code = res;
        response.success = false;
        syslog(LOG_ERR, "[ktv][network] Request failed: %s, error: %s", 
               url.c_str(), curl_easy_strerror(res));
    }
    
    return response;
}

// libcurl 回调函数（只负责收数据，不要有任何业务逻辑）
size_t NetworkService::writeCallback(void* contents, size_t size, size_t nmemb, void* userdata) {
    NetworkService* self = static_cast<NetworkService*>(userdata);
    size_t total_size = size * nmemb;
    
    // 检查缓冲区是否足够
    if (self->response_size_ + total_size >= MAX_RESPONSE_SIZE) {
        syslog(LOG_WARNING, "[ktv][network] Response buffer overflow");
        return 0;  // 返回 0 表示停止接收
    }
    
    // 只做 buffer append，不要有任何业务逻辑
    memcpy(self->response_buffer_ + self->response_size_, contents, total_size);
    self->response_size_ += total_size;
    
    return total_size;
}

size_t NetworkService::headerCallback(void* contents, size_t size, size_t nmemb, void* userdata) {
    // 如果需要处理响应头，在这里处理
    // 但不要有任何业务逻辑
    return size * nmemb;
}

void NetworkService::sendEvent(EventType type, int arg1, void* data) {
    AppEvent ev;
    ev.type = type;
    ev.arg1 = arg1;
    ev.data = data;
    EventQueue::instance().enqueue(ev);
}

void NetworkService::sendNetworkError(int curlCode, const std::string& url) {
    AppEvent ev;
    ev.type = EventType::EVENT_NETWORK_ERROR;
    ev.arg1 = curlCode;
    // 可以在这里传递错误信息（预分配内存）
    EventQueue::instance().enqueue(ev);
    
    syslog(LOG_ERR, "[ktv][network] Network error: %s, URL: %s", 
           curl_easy_strerror(static_cast<CURLcode>(curlCode)), url.c_str());
}
```

---

## 四、数据流详解

### 4.1 完整数据流示例：用户点击分类

```
┌─────────────────────────────────────────────────────────────────┐
│  1. Service 发起请求                                             │
│  CategoryService::onClick(categoryId)                          │
│    ↓                                                             │
│  NetworkService::instance().fetchCategory(categoryId)            │
└─────────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  2. NetworkService 调 libcurl（Network Worker 线程）              │
│  NetworkWorker::post([url, categoryId]() {                       │
│    HttpResponse response = performRequest(url);                  │
│      ↓                                                           │
│    curl_easy_perform(curl_)                                      │
│      ↓                                                           │
│    writeCallback()  // 只负责收数据                              │
│      ↓                                                           │
│    buffer_append()  // 拼 buffer                                │
│      ↓                                                           │
│    parseCategoryJson()  // 解析 JSON                            │
│      ↓                                                           │
│    sendEvent(EVENT_CATEGORY_DATA_READY, categoryId, data)       │
│  })                                                              │
└─────────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  3. EventQueue（事件队列）                                       │
│  EventQueue::enqueue(ev)                                        │
└─────────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  4. EventDispatcher（事件分发线程）                              │
│  case EVENT_CATEGORY_DATA_READY:                                │
│    CategoryService::onDataReady(ev.data);                       │
└─────────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  5. Service 处理结果                                             │
│  CategoryService::onDataReady(Data* data)                      │
│    • cache_save(data)  // 保存缓存                              │
│    • 决定 UI 行为                                                │
│    • 发送 UI 更新事件                                             │
└─────────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  6. UI 线程刷新界面                                              │
│  CategoryPresenter::onSvcCategoryDataReady(data)               │
│    ↓                                                             │
│  CategoryPage::renderCategoryList(data)                         │
│    • lv_label_set_text()  // UI 线程安全                        │
└─────────────────────────────────────────────────────────────────┘
```

### 4.2 关键点说明

1. **libcurl 回调只负责收数据**：
   ```cpp
   size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userdata) {
       // 只做 buffer append
       // 不要有任何业务逻辑
       // 不要调用 Service
       // 不要调用 UI
   }
   ```

2. **数据处理完 → push 一个 EVENT**：
   ```cpp
   HttpResponse response = performRequest(url);
   if (response.success) {
       void* data = parseJson(response.body);
       sendEvent(EVENT_CATEGORY_DATA_READY, categoryId, data);
   }
   ```

3. **Service 在事件线程里接**：
   ```cpp
   case EVENT_CATEGORY_DATA_READY:
       CategoryService::onDataReady(ev.data);
       break;
   ```

4. **UI 刷新一定在 UI 线程**：
   ```cpp
   void CategoryPage::renderCategoryList(Data* data) {
       // 在 UI 线程中调用
       lv_label_set_text(label, data->name.c_str());
   }
   ```

---

## 五、libcurl 回调的正确用法

### 5.1 libcurl 回调只干三件事

1. **收数据（write callback）**
2. **拼 buffer**
3. **返回 size**

```cpp
// ✅ 正确：只负责收数据
size_t NetworkService::writeCallback(void* contents, size_t size, size_t nmemb, void* userdata) {
    NetworkService* self = static_cast<NetworkService*>(userdata);
    size_t total_size = size * nmemb;
    
    // 检查缓冲区
    if (self->response_size_ + total_size >= MAX_RESPONSE_SIZE) {
        return 0;  // 停止接收
    }
    
    // 只做 buffer append
    memcpy(self->response_buffer_ + self->response_size_, contents, total_size);
    self->response_size_ += total_size;
    
    return total_size;
}
```

### 5.2 不要在回调里做的事

```cpp
// ❌ 错误：在回调里调用 Service
size_t writeCallback(...) {
    CategoryService::onData(...);  // 错误！
    return size;
}

// ❌ 错误：在回调里更新 UI
size_t writeCallback(...) {
    lv_label_set_text(...);  // 错误！
    return size;
}

// ❌ 错误：在回调里做业务逻辑
size_t writeCallback(...) {
    if (checkSomething()) {  // 错误！
        doSomething();
    }
    return size;
}
```

---

## 六、禁止事项（硬规则）

### 🚫 禁止1：在 UI 线程里直接 libcurl

```cpp
// ❌ 错误：在 UI 线程里直接 libcurl
void CategoryPage::onCategoryClick(lv_event_t* e) {
    CURL* curl = curl_easy_init();  // 错误！阻塞 UI
    curl_easy_perform(curl);        // 错误！卡顿
    curl_easy_cleanup(curl);
}

// ✅ 正确：通过 NetworkService
void CategoryPage::onCategoryClick(lv_event_t* e) {
    int categoryId = getCategoryId(e);
    NetworkService::instance().fetchCategory(categoryId);
}
```

### 🚫 禁止2：libcurl 回调直接打 Service / UI

```cpp
// ❌ 错误：回调直接打 Service
curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);
callback() {
    CategoryService::onData(...);  // 错误！
    lv_label_set_text(...);         // 错误！
}

// ✅ 正确：回调只收数据，通过 Event 驱动业务
curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
writeCallback() {
    buffer_append(...);  // 只收数据
}
// 请求完成后
sendEvent(EVENT_CATEGORY_DATA_READY, categoryId, data);
```

### 🚫 禁止3：多个 curl handle 并发

```cpp
// ❌ 错误：多个 curl handle
class NetworkService {
    CURL* curl1_;  // 错误！
    CURL* curl2_;  // 错误！
};

// ✅ 正确：只有一个 curl handle
class NetworkService {
    CURL* curl_;  // 只有一个
};
```

### 🚫 禁止4：在回调里做业务判断

```cpp
// ❌ 错误：在回调里做业务判断
size_t writeCallback(...) {
    if (isCategoryData()) {  // 错误！
        CategoryService::onData(...);
    } else if (isSearchData()) {  // 错误！
        SearchService::onData(...);
    }
    return size;
}

// ✅ 正确：请求完成后统一处理
void NetworkService::fetchCategory(int id) {
    HttpResponse response = performRequest(url);
    if (response.success) {
        sendEvent(EVENT_CATEGORY_DATA_READY, id, data);
    }
}
```

---

## 七、线程模型与类图

### 7.1 线程模型

```
┌─────────────────────────────────────────────────────────────────┐
│  UI 线程                                                         │
│  • CategoryPage::onCategoryClick()                               │
│  • 只塞 event 到 queue                                           │
└─────────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  EventDispatcher 线程                                            │
│  • 消费 EventQueue                                               │
│  • 路由到 Service                                                │
└─────────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  Network Worker 线程                                             │
│  • NetworkService::fetchCategory()                              │
│  • curl_easy_perform()                                           │
│  • writeCallback() 只收数据                                      │
│  • sendEvent() 发送结果事件                                       │
└─────────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  EventDispatcher 线程（接收结果）                                  │
│  • 接收 EVENT_CATEGORY_DATA_READY                                │
│  • 路由到 CategoryService::onDataReady()                        │
└─────────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  UI 线程（刷新界面）                                              │
│  • CategoryPage::renderCategoryList()                           │
│  • lvgl safe call                                               │
└─────────────────────────────────────────────────────────────────┘
```

### 7.2 类图

```
┌─────────────────────────────────────────────────────────────────┐
│  NetworkService (Singleton)                                      │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │  + instance() : NetworkService&                            │ │
│  │  + init() : bool                                           │ │
│  │  + cleanup() : void                                        │ │
│  │  + fetchCategory(id : int) : void                          │ │
│  │  + fetchSearch(keyword : string) : void                     │ │
│  │  - performRequest() : HttpResponse                         │ │
│  │  - writeCallback() : size_t (static)                        │ │
│  │  - sendEvent() : void                                      │ │
│  │  - curl_ : CURL* (只有一个)                                 │ │
│  └───────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                    │
                    │ 使用
                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  EventQueue (Singleton)                                         │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │  + instance() : EventQueue&                                 │ │
│  │  + enqueue(ev : AppEvent) : void                           │ │
│  │  + dequeue(ev : AppEvent&) : bool                          │ │
│  └───────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                    │
                    │ 使用
                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  EventDispatcher (Singleton)                                     │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │  + instance() : EventDispatcher&                           │ │
│  │  + start() : void                                          │ │
│  │  + stop() : void                                           │ │
│  │  - dispatchLoop() : void                                   │ │
│  │  - dispatch(ev : AppEvent) : void                         │ │
│  └───────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                    │
                    │ 调用
                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  CategoryService (Singleton)                                    │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │  + instance() : CategoryService&                          │ │
│  │  + onClick(id : int) : void                               │ │
│  │  + onDataReady(data : void*) : void                       │ │
│  └───────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

---

## 📚 相关文档

- [事件模型MVP实现指南（可落地版）.md](./事件模型MVP实现指南（可落地版）.md) ⭐⭐⭐ **必读**
- [应用层命名规范（架构约束版）.md](./应用层命名规范（架构约束版）.md) ⭐⭐⭐ **必读**
- [线程架构基线（最终版）.md](../线程架构基线（最终版）.md) ⭐⭐⭐ **必读**
- [工程规范完整指南（量产级）.md](./工程规范完整指南（量产级）.md) ⭐⭐⭐ **必读**

---

## 💡 总结

### 核心原则

1. **libcurl 全局只有一个**：只在 NetworkService，用 singleton
2. **返回值不要靠回调直接打 Service / UI**：通过 Event 驱动业务
3. **正解：网络线程 → push event → Service 收结果 → UI 刷新**

### MVP 级架构

- **NetworkService**：Singleton，只有一个 curl handle
- **网络线程**：1 个 Network Worker 线程
- **并发**：不要，串行请求
- **返回结果**：通过 Event 队列
- **业务处理**：在 Service 层
- **UI 刷新**：在 UI 线程
- **回调**：仅限 libcurl 内部，只负责收数据

### 避免回调地狱

> **"网络回调直接打业务，是嵌入式项目寿命 < 6 个月的标志。"**

**正确做法**：
- libcurl 回调只负责收数据
- 数据处理完 → push 一个 EVENT
- Service 在事件线程里接 → 决定怎么刷新 UI

---

**最后更新**: 2025-12-30  
**维护者**: 项目团队  
**状态**: ✅ 核心文档（MVP 可落地版）


