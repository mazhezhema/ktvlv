# src 源代码 Review 报告

> **生成时间**：2025-12-30  
> **审查标准**：基于最新架构规范和代码生成模板  
> **状态**：待调整

---

## 📋 概述

本报告基于最新的架构规范（NetworkService、EventQueue+EventDispatcher、命名规范）对 `src/` 目录下的源代码进行review，列出需要调整的地方。

---

## 🔴 必须调整的问题（高优先级）

### 1. HttpService → NetworkService（架构变更）

**问题**：
- `src/services/http_service.h` 和 `src/services/http_service.cpp` 使用同步接口
- 需要改为异步Event驱动的NetworkService

**当前实现**：
```cpp
// src/services/http_service.h
class HttpService {
public:
    static HttpService& getInstance();
    bool get(const char* url, HttpResponse& response);  // 同步接口
    bool post(const char* url, const char* json_data, HttpResponse& response);
};
```

**需要改为**：
```cpp
// src/services/network_service.h
class NetworkService {
public:
    static NetworkService& instance();
    bool init();  // 初始化
    void cleanup();
    void fetchCategory(int categoryId);  // 异步，结果通过Event返回
    void fetchSearch(const std::string& keyword);
    // ...
};
```

**影响范围**：
- `src/services/http_service.h` → 需要重命名为 `network_service.h`
- `src/services/http_service.cpp` → 需要重命名为 `network_service.cpp` 并重构
- `src/main.cpp`：`HttpService::getInstance()` → `NetworkService::instance()`
- `src/services/song_service.cpp`：使用了 HttpService
- `src/services/m3u8_download_service.cpp`：可能使用了 HttpService
- `src/services/log_upload_service.cpp`：可能使用了 HttpService

**相关文档**：
- [NetworkService与libcurl实现指南（MVP可落地版）.md](./guides/NetworkService与libcurl实现指南（MVP可落地版）.md) ⭐⭐⭐ **必读**
- [服务层API设计文档.md](./服务层API设计文档.md) ⭐⭐⭐ **必读**

---

### 2. EventBus → EventQueue + EventDispatcher（架构变更）

**问题**：
- `src/events/event_bus.h` 和 `src/events/event_bus.cpp` 使用EventBus模式
- `dispatchOnUiThread()` 在主线程中调用，但应该在独立的EventDispatcher线程中运行

**当前实现**：
```cpp
// src/events/event_bus.h
class EventBus {
public:
    static EventBus& getInstance();
    void publish(const Event& ev);
    bool poll(Event& ev);
    void dispatchOnUiThread();  // 在主线程调用
};
```

**需要改为**：
```cpp
// src/events/event_queue.h
class EventQueue {
public:
    static EventQueue& instance();
    void enqueue(const AppEvent& ev);  // 入队
    bool dequeue(AppEvent& ev, int timeout_ms = -1);  // 出队（阻塞）
};

// src/events/event_dispatcher.h
class EventDispatcher {
public:
    static EventDispatcher& instance();
    void start();  // 启动EventDispatcher线程
    void stop();
private:
    void dispatchLoop();  // EventDispatcher线程循环
    void dispatch(const AppEvent& ev);  // switch路由
};
```

**影响范围**：
- `src/events/event_bus.h` → 拆分为 `event_queue.h` 和 `event_dispatcher.h`
- `src/events/event_bus.cpp` → 拆分为 `event_queue.cpp` 和 `event_dispatcher.cpp`
- `src/main.cpp`：`EventBus::getInstance().dispatchOnUiThread()` → 需要启动EventDispatcher线程
- `src/services/player_service.cpp`：`EventBus::getInstance().publish()` → `EventQueue::instance().enqueue()`
- 其他使用 EventBus 的服务

**相关文档**：
- [事件模型MVP实现指南（可落地版）.md](./guides/事件模型MVP实现指南（可落地版）.md) ⭐⭐⭐ **必读**
- [事件架构规范.md](./architecture/事件架构规范.md) ⭐⭐ **参考**

---

### 3. Event 结构更新（数据结构变更）

**问题**：
- 当前 `Event` 结构使用 `std::string payload`，不符合MVP级简单设计
- 需要改为 `AppEvent`（type, arg1, arg2, data指针）

**当前实现**：
```cpp
// src/events/event_types.h
struct Event {
    EventType type{EventType::None};
    std::string payload;  // JSON字符串
};
```

**需要改为**：
```cpp
// src/events/event_types.h
enum class EventType {
    EVENT_CATEGORY_CLICK,
    EVENT_CATEGORY_DATA_READY,
    EVENT_SEARCH_SUBMITTED,
    EVENT_SEARCH_RESULT_READY,
    EVENT_NETWORK_ERROR,
    // ...
};

struct AppEvent {
    EventType type;
    int arg1 = 0;           // 通用参数1
    int arg2 = 0;           // 通用参数2
    void* data = nullptr;   // 可选数据指针（预分配内存）
};
```

**影响范围**：
- `src/events/event_types.h`：需要重构Event结构
- 所有使用 `Event` 的代码都需要更新为 `AppEvent`
- `EventBus::publish()` 的参数类型需要更新
- `dispatchOnUiThread()` 中的switch语句需要更新

**相关文档**：
- [事件模型MVP实现指南（可落地版）.md](./guides/事件模型MVP实现指南（可落地版）.md) ⭐⭐⭐ **必读**

---

### 4. 命名规范检查（代码风格）

**问题**：
- 需要检查是否有Controller/Presenter类使用了正确的命名规范
- UI入口应该使用 `onUiXxx`
- Service回调入口应该使用 `onSvcXxx`
- 内部函数应该使用 `handleXxx`

**当前状态**：
- 未发现明确的Controller/Presenter类
- 需要检查是否有类似的模式

**需要检查**：
- 是否有类名包含 `Controller` 或 `Presenter`
- 是否有方法名符合命名规范（onUiXxx, onSvcXxx, handleXxx）
- UI层是否有正确的命名

**相关文档**：
- [应用层命名规范（架构约束版）.md](./guides/应用层命名规范（架构约束版）.md) ⭐⭐⭐ **必读**

---

## 🟡 建议优化（中优先级）

### 5. Service Singleton模式统一

**问题**：
- 部分Service使用 `getInstance()`，部分使用 `instance()`
- 应该统一使用 `instance()`（符合新规范）

**当前实现**：
```cpp
// HttpService, SongService, LicenceService 等
static HttpService& getInstance() { ... }
```

**应该改为**：
```cpp
static HttpService& instance() { ... }
```

**影响范围**：
- `src/services/http_service.h`（即将改为NetworkService）
- `src/services/song_service.h`
- `src/services/licence_service.h`
- `src/services/player_service.h`
- `src/services/history_service.h`
- `src/services/m3u8_download_service.h`
- 所有使用 `getInstance()` 的代码

---

### 6. main.cpp中的服务初始化

**问题**：
- `src/main.cpp` 中直接调用 `HttpService::getInstance().initialize()`
- 需要改为 `NetworkService::instance().init()`
- 需要启动 EventDispatcher 线程

**当前实现**：
```cpp
// src/main.cpp
ktv::services::HttpService::getInstance().initialize(net_cfg.base_url, net_cfg.timeout);
```

**需要改为**：
```cpp
// src/main.cpp
ktv::services::NetworkService::instance().init();
ktv::events::EventDispatcher::instance().start();
```

---

## 📊 影响范围总结

### 需要修改的文件

1. **必须重命名/重构**：
   - `src/services/http_service.h` → `src/services/network_service.h`
   - `src/services/http_service.cpp` → `src/services/network_service.cpp`
   - `src/events/event_bus.h` → 拆分为 `event_queue.h` 和 `event_dispatcher.h`
   - `src/events/event_bus.cpp` → 拆分为 `event_queue.cpp` 和 `event_dispatcher.cpp`
   - `src/events/event_types.h` → 更新Event结构

2. **需要更新引用**：
   - `src/main.cpp`
   - `src/services/player_service.cpp`
   - `src/services/song_service.cpp`
   - `src/services/m3u8_download_service.cpp`
   - `src/services/log_upload_service.cpp`
   - 其他使用 HttpService 或 EventBus 的文件

3. **可能需要创建**：
   - `src/events/event_dispatcher.h`（新文件）
   - `src/events/event_dispatcher.cpp`（新文件）

---

## 📝 调整优先级

### 🔴 高优先级（必须立即调整）

1. **HttpService → NetworkService**：架构核心变更，影响所有网络请求
2. **EventBus → EventQueue + EventDispatcher**：事件系统核心变更，影响所有事件处理
3. **Event结构更新**：数据结构变更，影响所有事件相关代码

### 🟡 中优先级（建议尽快调整）

4. **Singleton命名统一**：代码风格统一，不影响功能但影响一致性
5. **main.cpp服务初始化**：需要配合架构变更一起调整

### 🟢 低优先级（可选优化）

6. **命名规范检查**：如果当前没有Controller/Presenter类，可以后续添加时遵循规范

---

## 📚 参考文档

- [NetworkService与libcurl实现指南（MVP可落地版）.md](./guides/NetworkService与libcurl实现指南（MVP可落地版）.md) ⭐⭐⭐ **必读**
- [事件模型MVP实现指南（可落地版）.md](./guides/事件模型MVP实现指南（可落地版）.md) ⭐⭐⭐ **必读**
- [应用层命名规范（架构约束版）.md](./guides/应用层命名规范（架构约束版）.md) ⭐⭐⭐ **必读**
- [服务层API设计文档.md](./服务层API设计文档.md) ⭐⭐⭐ **必读**
- [Cursor开发指南.md](./Cursor开发指南.md) ⭐⭐ **参考**

---

**最后更新**: 2025-12-30  
**状态**: ⚠️ 待调整

