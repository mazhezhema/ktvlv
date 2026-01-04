# Watchdog 决策模型与故障分级

> **文档版本**：v1.0  
> **最后更新**：2025-12-30  
> **状态**：✅ 核心文档（架构设计）  
> **适用平台**：F133 / Tina Linux  
> **目标**：明确 Watchdog 的决策权和三层故障级别，确保故障处理有明确的权力边界

---

## 🎯 核心原则（一句话）

> **三层故障级别：Level 1 模块自恢复，Level 2 重启 App，Level 3 系统重启。**

---

## 📋 目录

1. [故障分级](#一-故障分级)
2. [决策模型](#二-决策模型)
3. [Watchdog 职责](#三-watchdog-职责)
4. [实现示例](#四-实现示例)

---

## 一、故障分级

### 三层故障级别

| 级别 | 严重程度 | 影响范围 | 处理策略 | 决策权 |
|------|---------|---------|---------|--------|
| **Level 1** | ⚠️ 中等 | 单个模块 | 模块自恢复 | 模块自身 |
| **Level 2** | 🔴 高 | 整个应用 | 重启 App | AppRuntime |
| **Level 3** | 🔴 极高 | 整个系统 | 系统重启 | 系统级 Watchdog |

---

### Level 1：模块异常 → 模块自恢复

**触发条件**：
- 网络请求失败（可重试）
- 播放器临时卡顿（可恢复）
- UI 组件异常（可刷新）

**处理策略**：
- 模块内部重试（最多 N 次）
- 模块内部状态重置
- 模块内部资源清理

**决策权**：模块自身

**示例**：
```cpp
// NetworkWorker 内部重试
void NetworkWorker::handleHttpRequest(const HttpRequest& req) {
    int retry_count = 0;
    while (retry_count < MAX_RETRIES) {
        try {
            auto response = performHttpRequest(req);
            return response;  // 成功，返回
        } catch (const NetworkException& e) {
            retry_count++;
            if (retry_count >= MAX_RETRIES) {
                // 模块自恢复失败，上报 Level 2
                AppRuntime::instance().reportException(
                    ExceptionType::NETWORK_FAILURE,
                    ErrorCode::HTTP_REQUEST_FAILED,
                    "Network request failed after max retries"
                );
                return;
            }
            // 指数退避
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 * (1 << retry_count)));
        }
    }
}
```

---

### Level 2：模块不可恢复 → 重启 App

**触发条件**：
- 模块自恢复失败（超过最大重试次数）
- 模块线程异常退出
- 模块资源泄漏严重

**处理策略**：
- AppRuntime 记录异常日志
- AppRuntime 执行优雅退出
- 由 systemd 或外部脚本重启应用

**决策权**：AppRuntime

**示例**：
```cpp
void AppRuntime::handleLevel2Failure(ExceptionType type, ErrorCode code, const std::string& message) {
    syslog(LOG_ERR, "[ktv][watchdog] Level 2 failure detected: type=%d, code=%d, msg=%s",
           static_cast<int>(type), static_cast<int>(code), message.c_str());
    
    // 记录异常日志（用于远程上传）
    logException(type, code, message);
    
    // 执行优雅退出
    requestExit();
    
    // 退出码 1 表示需要重启
    exit(1);
}
```

---

### Level 3：App 不可恢复 → 系统重启

**触发条件**：
- App 连续重启失败（超过最大重启次数）
- 系统资源耗尽（内存、文件描述符）
- 硬件故障

**处理策略**：
- 系统级 Watchdog 触发
- 系统重启（由硬件 Watchdog 或 systemd 处理）

**决策权**：系统级 Watchdog

**示例**：
```systemd
# /etc/systemd/system/ktvlv.service
[Unit]
Description=KTV Application
After=network.target

[Service]
Type=simple
ExecStart=/opt/app/ktvlv
Restart=always
RestartSec=5
StartLimitIntervalSec=300
StartLimitBurst=5

# 如果 5 分钟内重启超过 5 次，触发系统重启
StartLimitAction=reboot-force
```

---

## 二、决策模型

### 决策流程

```
异常发生
    ↓
Level 1：模块自恢复
    ↓
自恢复成功？ ──是──→ 继续运行
    ↓ 否
Level 2：重启 App
    ↓
重启成功？ ──是──→ 继续运行
    ↓ 否（连续失败）
Level 3：系统重启
```

### 决策权边界

| 决策 | 决策者 | 触发条件 | 执行方式 |
|------|--------|---------|---------|
| **模块自恢复** | 模块自身 | 模块内部异常 | 模块内部逻辑 |
| **重启 App** | AppRuntime | 模块不可恢复 | AppRuntime::requestExit() |
| **系统重启** | 系统级 Watchdog | App 连续失败 | systemd / 硬件 Watchdog |

---

## 三、Watchdog 职责

### 应用级 Watchdog

**职责**：
- 监控关键模块的运行状态
- 检测模块异常退出
- 触发 Level 2 故障处理

**实现**：
```cpp
class AppWatchdog {
public:
    void start() {
        watchdog_thread_ = std::thread(&AppWatchdog::watchdogLoop, this);
    }

    void stop() {
        running_.store(false);
        if (watchdog_thread_.joinable()) {
            watchdog_thread_.join();
        }
    }

private:
    void watchdogLoop() {
        while (running_.load()) {
            // 检查关键模块状态
            checkModuleHealth();

            // 每 5 秒检查一次
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

    void checkModuleHealth() {
        // 检查 NetworkWorker
        if (!NetworkWorker::instance().isHealthy()) {
            syslog(LOG_ERR, "[ktv][watchdog] NetworkWorker unhealthy");
            AppRuntime::instance().reportException(
                ExceptionType::NETWORK_FAILURE,
                ErrorCode::MODULE_UNHEALTHY,
                "NetworkWorker unhealthy"
            );
        }

        // 检查 PlayerAdapter
        if (!PlayerAdapter::instance().isHealthy()) {
            syslog(LOG_ERR, "[ktv][watchdog] PlayerAdapter unhealthy");
            AppRuntime::instance().reportException(
                ExceptionType::PLAYER_HANG,
                ErrorCode::MODULE_UNHEALTHY,
                "PlayerAdapter unhealthy"
            );
        }
    }

    std::thread watchdog_thread_;
    std::atomic<bool> running_{false};
};
```

---

## 四、实现示例

### 完整决策模型

```cpp
class AppRuntime {
public:
    void reportException(ExceptionType type, ErrorCode code, const std::string& message) {
        syslog(LOG_ERR, "[ktv][runtime] Exception reported: type=%d, code=%d, msg=%s",
               static_cast<int>(type), static_cast<int>(code), message.c_str());

        // 记录异常日志
        logException(type, code, message);

        // 根据异常类型决策
        switch (type) {
            case ExceptionType::NETWORK_FAILURE:
                handleNetworkFailure(code, message);
                break;
            case ExceptionType::PLAYER_HANG:
                handlePlayerHang(code, message);
                break;
            case ExceptionType::UI_HANG:
                handleUIHang(code, message);
                break;
            default:
                // 默认 Level 2：重启 App
                handleLevel2Failure(type, code, message);
        }
    }

private:
    void handleNetworkFailure(ErrorCode code, const std::string& message) {
        static int restart_count = 0;
        const int max_restarts = 5;

        if (restart_count < max_restarts) {
            restart_count++;
            syslog(LOG_WARNING, "[ktv][runtime] Restarting NetworkWorker (%d/%d)", restart_count, max_restarts);

            // Level 1：尝试模块重启
            NetworkWorker::instance().stop();
            std::this_thread::sleep_for(std::chrono::seconds(3));
            NetworkWorker::instance().start();
        } else {
            // Level 2：重启 App
            syslog(LOG_ERR, "[ktv][runtime] NetworkWorker restart limit reached, restarting App");
            handleLevel2Failure(ExceptionType::NETWORK_FAILURE, code, message);
        }
    }

    void handleLevel2Failure(ExceptionType type, ErrorCode code, const std::string& message) {
        syslog(LOG_ERR, "[ktv][watchdog] Level 2 failure: type=%d, code=%d, msg=%s",
               static_cast<int>(type), static_cast<int>(code), message.c_str());

        // 执行优雅退出
        requestExit();

        // 退出码 1 表示需要重启
        exit(1);
    }
};
```

---

## 五、关键规则（必须遵守）

### 🚫 禁止事项

1. **禁止模块自行决定重启 App**
   - ❌ 禁止模块直接调用 `exit()` 或 `abort()`
   - ✅ 必须通过 AppRuntime 上报异常

2. **禁止跳过故障级别**
   - ❌ 禁止直接跳到 Level 3（系统重启）
   - ✅ 必须按 Level 1 → Level 2 → Level 3 顺序处理

3. **禁止无限重试**
   - ❌ 禁止无限制的模块重启
   - ✅ 必须设置最大重启次数

---

## 📚 相关文档

- [异常态处理总则.md](./异常态处理总则.md) ⭐⭐⭐ **必读**
- [AppRuntime线程生命周期总控设计.md](./AppRuntime线程生命周期总控设计.md) ⭐⭐⭐ **必读**
- [KTV_App稳定性与自愈设计说明.md](../sdk/KTV_App稳定性与自愈设计说明.md)

---

**最后更新**: 2025-12-30  
**维护者**: 项目团队  
**状态**: ✅ 核心文档（架构设计）

