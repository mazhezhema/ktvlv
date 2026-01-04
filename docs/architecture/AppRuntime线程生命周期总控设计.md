# AppRuntime 线程生命周期总控设计

> **文档版本**：v1.0  
> **最后更新**：2025-12-30  
> **状态**：✅ 核心文档（架构设计）  
> **适用平台**：F133 / Tina Linux  
> **目标**：定义全局线程生命周期总控点，确保所有线程的统一启动、停止和异常处理

---

## 🎯 核心原则（一句话）

> **所有 Singleton 线程由 AppRuntime 统一启动与回收，禁止各模块自行控制线程生命周期。**

---

## 📋 目录

1. [设计目标](#一-设计目标)
2. [AppRuntime 职责](#二-appruntime-职责)
3. [线程启动顺序](#三-线程启动顺序)
4. [线程停止顺序](#四-线程停止顺序)
5. [异常退出兜底](#五-异常退出兜底)
6. [实现示例](#六-实现示例)

---

## 一、设计目标

### 问题描述

**当前问题**：
- 各模块自行控制线程生命周期，启动/停止顺序不明确
- 异常退出时线程可能无法正确回收
- 缺乏统一的线程管理入口

**设计目标**：
- ✅ 统一线程生命周期管理入口
- ✅ 明确的启动/停止顺序
- ✅ 异常退出兜底机制
- ✅ 禁止各模块自行控制线程生命周期

---

## 二、AppRuntime 职责

### 核心职责

| 职责 | 说明 | 重要性 |
|------|------|--------|
| **线程启动顺序控制** | 按依赖关系启动所有 Singleton 线程 | ⭐⭐⭐ 必须 |
| **线程停止顺序控制** | 按依赖关系停止所有 Singleton 线程 | ⭐⭐⭐ 必须 |
| **异常退出兜底** | 异常情况下强制停止所有线程 | ⭐⭐⭐ 必须 |
| **线程状态监控** | 监控关键线程的运行状态 | ⭐⭐ 推荐 |

### 禁止事项

❌ **禁止各模块自行控制线程生命周期**
- 禁止在模块内部调用 `start()` / `stop()`
- 禁止在模块构造函数中启动线程
- 禁止在模块析构函数中停止线程

✅ **正确做法**：
- 所有线程的 `start()` / `stop()` 由 AppRuntime 统一调用
- 模块只负责实现线程逻辑，不负责生命周期管理

---

## 三、线程启动顺序

### 启动顺序原则

1. **依赖关系决定启动顺序**：被依赖的线程先启动
2. **核心线程优先**：UI、Event 等核心线程优先启动
3. **工作线程其次**：Network、Player 等工作线程其次启动
4. **守护线程最后**：LogUpload、Upgrade 等守护线程最后启动

### 标准启动顺序

```
1. EventBus（事件总线，所有模块依赖）
2. UISystem（UI主线程，所有UI操作依赖）
3. NetworkWorker（网络线程，业务模块可能依赖）
4. PlayerAdapter（播放器线程，播放功能依赖）
5. LogUploadService（日志上传，低优先级）
6. UpgradeService（升级检测，低优先级）
```

### 代码示例

```cpp
class AppRuntime {
public:
    static AppRuntime& instance() {
        static AppRuntime inst;
        return inst;
    }

    /**
     * @brief 启动所有线程（按依赖顺序）
     */
    void startAll() {
        syslog(LOG_INFO, "[ktv][runtime] Starting all threads...");
        
        // 1. EventBus（第1，所有模块依赖）
        EventBus::getInstance().start();
        
        // 2. UISystem（第2，UI主线程）
        UISystem::instance().start();
        
        // 3. NetworkWorker（第3，网络线程）
        NetworkWorker::instance().start();
        
        // 4. PlayerAdapter（第4，播放器线程）
        PlayerAdapter::instance().start();
        
        // 5. LogUploadService（第5，日志上传）
        LogUploadService::instance().start();
        
        // 6. UpgradeService（第6，升级检测）
        UpgradeService::instance().start();
        
        syslog(LOG_INFO, "[ktv][runtime] All threads started");
    }

private:
    AppRuntime() = default;
    ~AppRuntime() = default;
};
```

---

## 四、线程停止顺序

### 停止顺序原则

1. **依赖关系决定停止顺序**：依赖者先停止
2. **守护线程优先**：LogUpload、Upgrade 等守护线程优先停止
3. **工作线程其次**：Network、Player 等工作线程其次停止
4. **核心线程最后**：Event、UI 等核心线程最后停止

### 标准停止顺序

```
1. UpgradeService（升级检测，低优先级）
2. LogUploadService（日志上传，低优先级）
3. PlayerAdapter（播放器线程，依赖者先停止）
4. NetworkWorker（网络线程，依赖者先停止）
5. EventBus（事件总线，被依赖者后停止）
6. UISystem（UI主线程，最后停止）
```

### 代码示例

```cpp
/**
 * @brief 停止所有线程（按依赖顺序）
 */
void stopAll() {
    syslog(LOG_INFO, "[ktv][runtime] Stopping all threads...");
    
    // 1. UpgradeService（第1，低优先级）
    UpgradeService::instance().stop();
    
    // 2. LogUploadService（第2，低优先级）
    LogUploadService::instance().stop();
    
    // 3. PlayerAdapter（第3，播放器线程）
    PlayerAdapter::instance().stop();
    
    // 4. NetworkWorker（第4，网络线程）
    NetworkWorker::instance().stop();
    
    // 5. EventBus（第5，事件总线）
    EventBus::getInstance().stop();
    
    // 6. UISystem（第6，UI主线程，最后停止）
    UISystem::instance().stop();
    
    syslog(LOG_INFO, "[ktv][runtime] All threads stopped");
}
```

---

## 五、异常退出兜底

### 异常退出场景

1. **信号处理**：SIGTERM、SIGINT 等信号
2. **异常捕获**：未捕获的异常
3. **Watchdog 触发**：进程级 Watchdog 触发

### 兜底机制

```cpp
class AppRuntime {
public:
    /**
     * @brief 异常退出兜底（强制停止所有线程）
     */
    void emergencyStop() {
        syslog(LOG_ERR, "[ktv][runtime] Emergency stop triggered!");
        
        // 设置全局退出标志
        g_app_quit = true;
        
        // 强制停止所有线程（不等待，直接通知）
        UpgradeService::instance().stop();
        LogUploadService::instance().stop();
        PlayerAdapter::instance().stop();
        NetworkWorker::instance().stop();
        EventBus::getInstance().stop();
        UISystem::instance().stop();
        
        syslog(LOG_ERR, "[ktv][runtime] Emergency stop completed");
    }

    /**
     * @brief 注册信号处理
     */
    void registerSignalHandlers() {
        signal(SIGTERM, [](int sig) {
            syslog(LOG_WARNING, "[ktv][runtime] Received SIGTERM, stopping...");
            AppRuntime::instance().emergencyStop();
            exit(0);
        });
        
        signal(SIGINT, [](int sig) {
            syslog(LOG_WARNING, "[ktv][runtime] Received SIGINT, stopping...");
            AppRuntime::instance().emergencyStop();
            exit(0);
        });
    }
};
```

---

## 六、实现示例

### 完整实现示例

```cpp
#include <signal.h>
#include <syslog.h>
#include <atomic>

// 全局退出标志
static std::atomic<bool> g_app_quit{false};

class AppRuntime {
public:
    static AppRuntime& instance() {
        static AppRuntime inst;
        return inst;
    }

    AppRuntime(const AppRuntime&) = delete;
    AppRuntime& operator=(const AppRuntime&) = delete;

    /**
     * @brief 初始化 AppRuntime
     */
    void init() {
        // 注册信号处理
        registerSignalHandlers();
        
        // 设置异常处理
        std::set_terminate([]() {
            syslog(LOG_ERR, "[ktv][runtime] Unhandled exception, emergency stop!");
            AppRuntime::instance().emergencyStop();
            std::abort();
        });
    }

    /**
     * @brief 启动所有线程
     */
    void startAll() {
        syslog(LOG_INFO, "[ktv][runtime] Starting all threads...");
        
        try {
            EventBus::getInstance().start();
            UISystem::instance().start();
            NetworkWorker::instance().start();
            PlayerAdapter::instance().start();
            LogUploadService::instance().start();
            UpgradeService::instance().start();
            
            syslog(LOG_INFO, "[ktv][runtime] All threads started");
        } catch (const std::exception& e) {
            syslog(LOG_ERR, "[ktv][runtime] Failed to start threads: %s", e.what());
            emergencyStop();
            throw;
        }
    }

    /**
     * @brief 停止所有线程
     */
    void stopAll() {
        syslog(LOG_INFO, "[ktv][runtime] Stopping all threads...");
        
        g_app_quit = true;
        
        UpgradeService::instance().stop();
        LogUploadService::instance().stop();
        PlayerAdapter::instance().stop();
        NetworkWorker::instance().stop();
        EventBus::getInstance().stop();
        UISystem::instance().stop();
        
        syslog(LOG_INFO, "[ktv][runtime] All threads stopped");
    }

    /**
     * @brief 异常退出兜底
     */
    void emergencyStop() {
        syslog(LOG_ERR, "[ktv][runtime] Emergency stop triggered!");
        
        g_app_quit = true;
        
        // 强制停止（不等待）
        UpgradeService::instance().stop();
        LogUploadService::instance().stop();
        PlayerAdapter::instance().stop();
        NetworkWorker::instance().stop();
        EventBus::getInstance().stop();
        UISystem::instance().stop();
        
        syslog(LOG_ERR, "[ktv][runtime] Emergency stop completed");
    }

    /**
     * @brief 检查退出标志
     */
    bool shouldQuit() const {
        return g_app_quit.load();
    }

private:
    AppRuntime() = default;
    ~AppRuntime() = default;

    void registerSignalHandlers() {
        signal(SIGTERM, [](int sig) {
            syslog(LOG_WARNING, "[ktv][runtime] Received SIGTERM");
            AppRuntime::instance().emergencyStop();
            exit(0);
        });
        
        signal(SIGINT, [](int sig) {
            syslog(LOG_WARNING, "[ktv][runtime] Received SIGINT");
            AppRuntime::instance().emergencyStop();
            exit(0);
        });
    }
};

// 使用示例
int main() {
    // 初始化 AppRuntime
    AppRuntime::instance().init();
    
    // 启动所有线程
    AppRuntime::instance().startAll();
    
    // 主循环
    while (!AppRuntime::instance().shouldQuit()) {
        // UI 主循环
        lv_timer_handler();
        usleep(5000);
    }
    
    // 停止所有线程
    AppRuntime::instance().stopAll();
    
    return 0;
}
```

---

## 七、关键规则（必须遵守）

### 🚫 禁止事项

1. **禁止各模块自行控制线程生命周期**
   - ❌ 禁止在模块内部调用 `start()` / `stop()`
   - ❌ 禁止在模块构造函数中启动线程
   - ❌ 禁止在模块析构函数中停止线程

2. **禁止绕过 AppRuntime 启动/停止线程**
   - ❌ 禁止直接调用 `NetworkWorker::instance().start()`
   - ✅ 必须通过 `AppRuntime::instance().startAll()` 启动

3. **禁止在异常处理中忽略线程停止**
   - ❌ 禁止捕获异常后不停止线程
   - ✅ 必须调用 `emergencyStop()` 确保线程回收

---

## 📚 相关文档

- [线程架构基线（最终版）.md](../线程架构基线（最终版）.md)
- [KTV_App线程Singleton编码规范（最终版）.md](../guides/KTV_App线程Singleton编码规范（最终版）.md)
- [KTV_App稳定性与自愈设计说明.md](../sdk/KTV_App稳定性与自愈设计说明.md)

---

**最后更新**: 2025-12-30  
**维护者**: 项目团队  
**状态**: ✅ 核心文档（架构设计）

