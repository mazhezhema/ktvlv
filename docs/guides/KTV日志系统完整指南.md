# KTV 日志系统完整指南（F133 / Tina Linux / syslog）

> **文档版本**：v2.0（合并版）  
> **最后更新**：2025-12-30  
> **状态**：✅ 核心文档（合并版）  
> **适用平台**：F133 / Tina Linux（BusyBox 体系）  
> **目标**：量产可排障、低开销、可 grep、可上传、不中断 UI/播放

---

## 🎯 一句话结论（先拍板）

> **syslog 是 F133 上最稳妥、最成熟、最低维护成本的日志方案。**  
> **前提是：你只用它做"记录"，不让它背业务责任，上传逻辑完全解耦。**

---

## 📋 目录

1. [syslog 全链路架构](#一-syslog-全链路架构)
2. [日志使用规范](#二-日志使用规范)
3. [LogUploadService 设计](#三-loguploadservice-设计)
4. [实现示例](#四-实现示例)
5. [最佳实践与避坑](#五-最佳实践与避坑)

---

## 一、syslog 全链路架构

### 1.1 F133/Tina Linux 里的 syslog 全链路

在 F133（Tina Linux，BusyBox 体系）里，**日志不是一个 API，而是一条流水线**：

```
[你的 KTV App]
   |
   | syslog() / openlog()
   v
[/dev/log]  (UNIX domain socket)
   |
   v
[syslogd]  (守护进程)
   |
   | ring buffer / 可选落盘
   v
[logread]  (读取工具)
   |
   v
[你自己的 LogUploadService / shell]
```

这条链路是**系统级设施**，不是你自己拼的。

### 1.2 syslog 在 F133 上"到底是啥"

#### syslog = 标准日志 API（你唯一该用的入口）

在代码里：

```c
#include <syslog.h>

openlog("ktv", LOG_PID | LOG_NDELAY, LOG_USER);
syslog(LOG_INFO, "player start song_id=%s", id);
closelog();
```

**这一步做了什么？**

* 把日志写到 `/dev/log`
* **不是文件**
* **不是你自己管理 buffer**

👉 **这是关键：syslog 不关心存哪，只负责"投递"**

#### `/dev/log` 是什么（不要碰）

* UNIX Domain Socket
* syslogd 在监听
* 协议复杂
* BusyBox 私有实现

❌ **严禁**

* 自己 open/read `/dev/log`
* 自己实现 syslog client

#### syslogd（F133 上的核心角色）

**syslogd 是什么？**

* BusyBox 提供的守护进程
* 系统启动时自动拉起
* 接收所有 syslog()

**syslogd 默认行为（Tina 常见）**

* **内存 ring buffer**
* 可配置：
  * 是否落盘
  * buffer 大小
  * 过滤级别

👉 **重点**：

> **syslogd 已经是"独立进程 + 缓冲 + 串行写"**

你再搞一层线程，是**重复设计**。

### 1.3 logread 是什么

**logread 的真实角色**

> **logread = syslogd 的"官方查询接口"**

它做的事：

* 从 syslogd 的 ring buffer 读日志
* 格式化输出
* 不解析 socket
* 不关心你业务

你可以把它理解为：

> **syslogd 的只读 API，只是表现形式是 CLI**

**logread 在 F133 上的常见形态**

在 Tina / BusyBox 里：

```sh
logread                    # 读取当前日志
logread -f                 # 类似 tail -f（慎用）
logread | grep "[ktv]"     # 过滤你的日志
```

特点：

* **不保证历史**
* 重启即丢
* 不适合长期存档

👉 这正符合 **嵌入式设备的现实约束**

### 1.4 syslog / logread 在 KTV 项目里的"正确分工"

#### 1️⃣ KTV App（你的代码）应该怎么做？

**✅ 只做一件事：打点日志**

```c
#include <syslog.h>

// 启动时一次即可
openlog("ktv", LOG_PID | LOG_NDELAY, LOG_USER);

// 使用日志
syslog(LOG_INFO, "[ktv][player] start song=%s", id);
syslog(LOG_ERR,  "[ktv][net] request failed code=%d", err);
syslog(LOG_WARNING, "[ktv][ui] page switch timeout");
```

**规范建议**

* 统一 tag：`[ktv][module]`
* 不打大对象
* 不拼 JSON
* 不高频

❌ 不做：

* 文件写
* 上传
* 线程管理

#### 2️⃣ syslogd（系统）做什么？

* 接收
* 排队
* 缓冲
* 过滤

👉 **你信它就行**

#### 3️⃣ LogUploadService（你要写的）做什么？

这是你唯一"介入"的地方。

**推荐职责**

* 触发式拉取日志
* 过滤 `[ktv]`
* 合并
* 低频上传

**推荐实现方式**

```cpp
// LogUploadService::collectLogs()
FILE* fp = popen("logread", "r");
if (!fp) {
    syslog(LOG_ERR, "[ktv][log] logread failed");
    return false;
}

std::vector<std::string> logs;
char line[1024];
while (fgets(line, sizeof(line), fp)) {
    if (strstr(line, "[ktv]")) {
        logs.push_back(line);
    }
}
pclose(fp);

// 合并并上传
return uploadLogs(logs);
```

---

## 二、日志使用规范

### 2.1 核心原则

1. **syslog 是唯一正式日志入口**：所有模块统一走 syslog()
2. **日志"事件化"，不"过程化"**：记录状态迁移、错误、关键动作
3. **禁止在实时路径打日志**：UI tick / 音频回调 / 播放回调严禁 syslog
4. **生产版本关闭 DEBUG**：仅保留 INFO/WARN/ERR（按策略）
5. **日志不承担业务**：日志失败不影响业务流程
6. **默认不落盘**：避免 Flash/eMMC 写放大；必要时仅"按需导出"

### 2.2 日志格式标准

#### 统一 Tag

统一格式：

```
[ktv][模块][事件] key=value key=value ...
```

**示例**：

```
[ktv][player][start] song_id=123 url_hash=ab12
[ktv][net][http_fail] path=/songs/search code=504 retry=1
[ktv][ui][page_enter] page=home
[ktv][storage][history_load_ok] count=30
```

#### 必须使用 key=value

- 便于 grep / 解析 / 上传结构化
- 允许用短 key：`song_id`, `code`, `err`, `page`, `ms`

### 2.3 日志级别约定

| syslog level | 使用场景 | 是否建议上传 |
|-------------|---------|------------|
| **LOG_ERR** | 播放失败、解码失败、关键资源加载失败、崩溃前关键点 | 必传 |
| **LOG_WARNING** | 网络慢/超时重试、资源不足、降级策略触发 | 条件传 |
| **LOG_INFO** | 状态变化、关键业务动作、启动阶段里程碑 | 条件传 |
| **LOG_DEBUG** | 仅开发调试；生产关闭 | 不传 |

### 2.4 哪些线程/回调禁止打日志（红线）

#### ❌ 禁止 syslog 的场景

- LVGL tick / lv_timer 回调
- UI 频繁事件（滑动、动画帧）
- 音频回调线程
- 播放器高频回调（progress / frame ready）
- 任何 while(1) 高频循环

#### ✅ 允许 syslog 的场景（低频）

- 状态切换（Idle -> Loading -> Playing -> Error）
- 播放开始/结束/失败
- HTTP 请求失败/重试/成功摘要
- 存储加载/保存结果
- 用户关键动作（点歌、切歌、收藏）

### 2.5 各模块必打日志点（建议清单）

#### App 生命周期

```
[start] app_start version=...
[ready] lvgl_ok player_ok net_ok
[exit] app_exit reason=...
```

#### Player

```
[start] song_id=... bitrate=... track=...
[switch] from=... to=...
[fail] song_id=... err=... stage=open/decode/buffer
[finish] song_id=... ms=...
```

#### Network

```
[req] method=GET path=... rid=...
[http_fail] rid=... code=... retry=... ms=...
[ok] rid=... code=200 ms=... bytes=...
```

#### Storage（历史/收藏）

```
[history_load_ok] count=...
[history_load_fail] err=...
[history_save_ok] count=...
[history_save_fail] err=...
```

#### UI（只允许低频事件）

```
[page_enter] page=...
[action] name=play_click song_id=...
```

### 2.6 日志封装宏规范（建议）

- 统一 openlog/closelog 生命周期：启动一次、退出一次
- 统一输出格式，避免散落 printf
- 统一编译期开关：生产关闭 DEBUG

**示例**：

```c
// 启动时
openlog("ktv", LOG_PID | LOG_NDELAY, LOG_USER);

// 使用
syslog(LOG_INFO, "[ktv][player][start] song_id=%s", song_id);
syslog(LOG_ERR, "[ktv][net][http_fail] path=%s code=%d", path, code);

// 退出时
closelog();
```

### 2.7 Code Review Checklist（必须检查）

- [ ] 是否在 UI tick / 音频回调打日志？（必须否）
- [ ] 是否用 `[ktv][module][event]` 格式？（必须是）
- [ ] 是否带 key=value？（必须是）
- [ ] 是否遗漏 coredump/关键失败日志点？（必须补）
- [ ] 是否在生产版本输出 DEBUG？（必须否）
- [ ] 是否包含隐私/密钥？（必须否）

---

## 三、LogUploadService 设计

### 3.1 目标

- 不影响 UI/播放（低优先级、低频）
- 支持"按需抓取 + 批量上传"
- 支持离线缓存（可选）与网络恢复补传（可选）
- 支持一键导出（用户反馈/售后）

### 3.2 组件与线程

#### 线程划分

- **主线程**：UI/LVGL loop（禁止日志抓取/上传）
- **Player 线程**：播放控制（只打 syslog，不做上传）
- **Network 线程**：HTTP 请求（只打 syslog，不做上传）
- **LogUpload 线程（新增，低优先级）**
  - 任务：抓取 logread 输出、过滤、打包、上传
  - 唤醒：事件触发 + 定时器（低频）

#### 交互方式

- 各模块不直接调用 logread
- 各模块只"发信号"给 LogUploadService：
  - push 一个 UploadReason（枚举）
  - 或设置一个"需要上传"的标记位

**推荐**：`std::queue + std::mutex + condition_variable`（MVP 够用）

### 3.3 状态机

#### 状态定义

```
IDLE
  - 无上传任务，阻塞等待 condvar / 超时

COLLECTING
  - popen("logread") 获取日志
  - 过滤 [ktv]
  - 截断上限（避免爆内存）

PACKING
  - 追加设备信息：device_id / fw_version / uptime
  - 可选 gzip 压缩（若你们已有轻量实现，否则先不做）

UPLOADING
  - 使用 libcurl 上传（POST）
  - 超时、重试、失败降级

DONE / FAIL
  - 成功：回到 IDLE
  - 失败：进入 BACKOFF

BACKOFF
  - 指数退避 sleep（例如 10s, 30s, 60s）
  - 到点回到 IDLE
```

#### 状态转换图

```
[IDLE]  ←→  [COLLECTING]  →  [PACKING]  →  [UPLOADING]  →  [IDLE]
  ↑                                                              ↓
  └────────── [BACKOFF] ←──────────────────────────────────────┘
```

### 3.4 触发策略（建议）

#### 触发源（谁发任务）

- **PlayerService**：
  - 播放失败（ERR）
  - 连续 3 次解码失败
- **NetworkService**：
  - 连续 N 次 http_fail
- **UI**：
  - 用户点击"反馈/导出日志"
- **系统**：
  - 每 X 小时低频上传一次摘要（可选，不建议 MVP 开）

#### 触发合并策略

- 10 分钟内多次触发 → 合并为一次上传（避免刷屏/刷网）

### 3.5 数据上限保护（必须）

- 单次抓取最大行数：例如 2000 行
- 单次抓取最大字节：例如 256KB（硬上限）
- 单次上传最大包体：例如 512KB
- 超出则截断，并记录：
  - `[ktv][log][truncate] bytes=... lines=...`

### 3.6 脱敏规则（必须）

上传前必须做：

- 替换 token / auth header / cookie
- URL 去 query（保留 path）
- 设备序列号可 hash

### 3.7 最简接口（给上层调用）

```cpp
class LogUploadService {
public:
    static LogUploadService& instance();
    
    // 通知上传（非阻塞）
    void Notify(UploadReason reason);
    
    // 一键导出到文件（可选）
    bool ExportToFile(const char* path);
    
    // 启动/停止服务线程
    void Start();
    void Stop();
    
private:
    enum class UploadReason {
        PLAYER_ERROR,      // 播放失败
        NETWORK_ERROR,     // 连续网络错误
        USER_FEEDBACK,     // 用户反馈
        ADMIN_COMMAND,     // 运维指令
        PERIODIC           // 定时上传（可选）
    };
};
```

**上层调用者**

- PlayerService / NetworkService / UI（仅通知，不传日志内容）

**示例**：

```cpp
// PlayerService 播放失败时
void PlayerService::onError(int code) {
    syslog(LOG_ERR, "[ktv][player][fail] err=%d", code);
    LogUploadService::instance().Notify(UploadReason::PLAYER_ERROR);
}

// UI 用户点击反馈
void FeedbackPage::onSubmit() {
    LogUploadService::instance().Notify(UploadReason::USER_FEEDBACK);
}
```

---

## 四、实现示例

### 4.1 日志收集函数

#### log_collect.h

```cpp
// log_collect.h
#pragma once
#include <string>

struct LogCollectConfig {
    size_t max_bytes = 256 * 1024;   // 256KB hard limit
    size_t max_lines = 2000;         // hard limit
    const char* include_keyword = "[ktv]";
};

/**
 * 收集 KTV 相关日志
 * @param out 输出字符串（收集到的日志）
 * @param cfg 配置（上限、过滤关键词）
 * @return true 成功，false 失败
 */
bool CollectKtvLogs(std::string& out, const LogCollectConfig& cfg);
```

#### log_collect.cpp

```cpp
// log_collect.cpp
#include "log_collect.h"
#include <cstdio>
#include <cstring>

bool CollectKtvLogs(std::string& out, const LogCollectConfig& cfg) {
    out.clear();

    FILE* fp = popen("logread", "r");
    if (!fp) {
        return false;
    }

    char line[512];
    size_t lines = 0;

    while (fgets(line, sizeof(line), fp)) {
        // 过滤关键词
        if (cfg.include_keyword && std::strstr(line, cfg.include_keyword) == nullptr) {
            continue;
        }

        size_t len = std::strlen(line);
        
        // 检查字节上限
        if (out.size() + len > cfg.max_bytes) {
            // 截断：不再追加
            out.append("[ktv][log][truncate] reason=max_bytes\n");
            break;
        }

        out.append(line);
        lines++;
        
        // 检查行数上限
        if (lines >= cfg.max_lines) {
            out.append("[ktv][log][truncate] reason=max_lines\n");
            break;
        }
    }

    int ret = pclose(fp);
    if (ret != 0) {
        return false;
    }

    return true;
}
```

### 4.2 JSON 打包函数

> **说明**：上传 payload 这块，用字符串拼 JSON 完全 OK（你们已固定 REST 协议，字段少），不用强行 cJSON。  
> cJSON 更适合"解析服务器返回"。

#### log_pack.h

```cpp
// log_pack.h
#pragma once
#include <string>

/**
 * 构建上传 payload JSON
 * @param device_id 设备ID
 * @param fw_version 固件版本
 * @param uptime_sec 运行时间（秒）
 * @param logs_text 日志文本
 * @return JSON 字符串
 */
std::string BuildUploadPayloadJson(const char* device_id,
                                  const char* fw_version,
                                  long uptime_sec,
                                  const std::string& logs_text);
```

#### log_pack.cpp

```cpp
// log_pack.cpp
#include "log_pack.h"
#include <cstdio>
#include <string>

static std::string EscapeJson(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 64);
    for (char c : s) {
        switch (c) {
            case '\\': o += "\\\\"; break;
            case '"':  o += "\\\""; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:   o += c;      break;
        }
    }
    return o;
}

std::string BuildUploadPayloadJson(const char* device_id,
                                  const char* fw_version,
                                  long uptime_sec,
                                  const std::string& logs_text) {
    char hdr[256];
    std::snprintf(hdr, sizeof(hdr),
        "{\"device_id\":\"%s\",\"fw_version\":\"%s\",\"uptime\":%ld,\"logs\":\"",
        device_id ? device_id : "unknown",
        fw_version ? fw_version : "unknown",
        uptime_sec);

    std::string out = hdr;
    out += EscapeJson(logs_text);
    out += "\"}";
    return out;
}
```

### 4.3 完整使用示例

```cpp
#include "log_collect.h"
#include "log_pack.h"
#include "log_upload.h"

bool UploadKtvLogs() {
    // 1. 收集日志
    LogCollectConfig cfg;
    cfg.max_bytes = 256 * 1024;
    cfg.max_lines = 2000;
    cfg.include_keyword = "[ktv]";
    
    std::string logs;
    if (!CollectKtvLogs(logs, cfg)) {
        syslog(LOG_ERR, "[ktv][log] collect logs failed");
        return false;
    }
    
    if (logs.empty()) {
        syslog(LOG_INFO, "[ktv][log] no logs to upload");
        return true;
    }
    
    // 2. 打包 JSON
    const char* device_id = "F133-001";
    const char* fw_version = "1.0.0";
    long uptime_sec = time(nullptr) - start_time;
    
    std::string payload = BuildUploadPayloadJson(
        device_id, fw_version, uptime_sec, logs);
    
    // 3. 上传
    if (!UploadLogsToServer(payload)) {
        syslog(LOG_ERR, "[ktv][log] upload failed");
        return false;
    }
    
    syslog(LOG_INFO, "[ktv][log] upload success");
    return true;
}
```

---

## 五、最佳实践与避坑

### 5.1 为什么 syslog 本身绝不能单独线程？

这是很多人会犯的错。

#### ❌ 错误模型

```
业务线程 → log queue → log thread → syslog()
```

#### 为什么错？

* syslogd 已经是队列
* 你加线程 = 延迟 + 顺序错乱
* 崩溃前日志反而没刷进去

👉 **syslog 就是"同步点"**

#### ✅ 正确模型

```
业务线程 → syslog() → syslogd (系统进程)
```

**直接调用，不要包装线程。**

### 5.2 F133/Tina 下 syslog 的最佳实践

#### ✅ 最佳实践 1：统一 openlog

```c
// main.cpp 启动时
openlog("ktv", LOG_PID | LOG_NDELAY, LOG_USER);
```

启动时一次即可。

#### ✅ 最佳实践 2：模块化 tag

```c
syslog(LOG_INFO, "[ktv][ui] page switch to home");
syslog(LOG_INFO, "[ktv][player] start song_id=%s", id);
syslog(LOG_ERR,  "[ktv][net] request failed code=%d", err);
syslog(LOG_WARNING, "[ktv][db] cache miss key=%s", key);
```

logread 过滤非常好用：

```sh
logread | grep "[ktv][player]"  # 只看播放器日志
logread | grep "[ktv][net]"     # 只看网络日志
```

#### ✅ 最佳实践 3：控制级别

| 级别 | 用途 | 示例 |
|------|------|------|
| **LOG_ERR** | 必传 | 播放失败、网络错误 |
| **LOG_WARNING** | 条件传 | 超时、重试 |
| **LOG_INFO** | 调试 | 状态切换、关键操作 |
| **LOG_DEBUG** | 生产关闭 | 详细调试信息 |

#### ✅ 最佳实践 4：日志上传"按需"

触发条件：

* 播放失败
* 连续网络错误
* 用户反馈
* 运维命令

```cpp
// 播放失败时触发上传
void PlayerAdapter::onError(int code) {
    syslog(LOG_ERR, "[ktv][player] error code=%d", code);
    LogUploadService::instance().triggerUpload();
}
```

#### ✅ 最佳实践 5：不要长期落盘

Flash 写放大 + 寿命问题。

**Tina 配置建议**：

* syslogd 只使用内存 ring buffer
* 不配置持久化存储
* 日志上传后即丢弃

### 5.3 必须提前规避的 10 个坑

#### ❌ 坑 1：把 syslog 当数据库

它不是持久化系统。

**错误示例**：

```c
// ❌ 错误：用 syslog 存储业务数据
syslog(LOG_INFO, "user_id=%d, song_id=%d, play_time=%d", ...);
```

**正确做法**：

```c
// ✅ 正确：只记录关键事件
syslog(LOG_INFO, "[ktv][player] user play song_id=%d", song_id);
```

#### ❌ 坑 2：高频打 log

UI tick / 播放回调禁止。

**错误示例**：

```c
// ❌ 错误：在 UI tick 中打 log
void ui_tick() {
    syslog(LOG_DEBUG, "[ktv][ui] tick");  // 禁止！
}
```

**正确做法**：

```c
// ✅ 正确：只在关键事件打 log
void onPageSwitch(const char* page) {
    syslog(LOG_INFO, "[ktv][ui] page switch to %s", page);
}
```

#### ❌ 坑 3：在音频线程打 log

直接影响实时性。

**错误示例**：

```c
// ❌ 错误：在音频回调中打 log
void audio_callback(void* data, int len) {
    syslog(LOG_DEBUG, "[ktv][audio] callback len=%d", len);  // 禁止！
}
```

**正确做法**：

```c
// ✅ 正确：音频线程不打 log，或只在错误时打
void audio_callback(void* data, int len) {
    // 正常流程不打 log
    if (error) {
        syslog(LOG_ERR, "[ktv][audio] error");  // 只在错误时
    }
}
```

#### ❌ 坑 4：实时上传

这是功耗 + 稳定性双杀。

**错误示例**：

```c
// ❌ 错误：每次打 log 都上传
void log_and_upload(const char* msg) {
    syslog(LOG_INFO, msg);
    LogUploadService::instance().triggerUpload();  // 禁止！
}
```

**正确做法**：

```c
// ✅ 正确：按需触发上传
void onCriticalError() {
    syslog(LOG_ERR, "[ktv] critical error");
    LogUploadService::instance().triggerUpload();  // 只在关键错误时
}
```

#### ❌ 坑 5：logread -f 常驻

CPU + 电源杀手。

**错误示例**：

```cpp
// ❌ 错误：常驻 logread -f
void LogUploadService::threadLoop() {
    FILE* fp = popen("logread -f", "r");  // 禁止！
    // ...
}
```

**正确做法**：

```cpp
// ✅ 正确：按需执行 logread
bool LogUploadService::collectLogs(std::string& out) {
    FILE* fp = popen("logread", "r");  // 一次性读取
    // ...
}
```

#### ❌ 坑 6：日志带 token / url

安全事故源头。

**错误示例**：

```c
// ❌ 错误：日志包含敏感信息
syslog(LOG_INFO, "[ktv][net] request url=%s, token=%s", url, token);
```

**正确做法**：

```c
// ✅ 正确：日志不包含敏感信息
syslog(LOG_INFO, "[ktv][net] request endpoint=/api/songs");
```

#### ❌ 坑 7：生产版本开 DEBUG

必出事。

**错误示例**：

```c
// ❌ 错误：生产版本打 DEBUG 日志
#ifdef DEBUG
    syslog(LOG_DEBUG, "[ktv] debug info");
#endif
```

**正确做法**：

```c
// ✅ 正确：生产版本关闭 DEBUG
#if defined(DEBUG) && DEBUG_LEVEL >= 2
    syslog(LOG_DEBUG, "[ktv] debug info");
#endif
```

#### ❌ 坑 8：日志格式随意

后期无法自动分析。

**错误示例**：

```c
// ❌ 错误：格式不统一
syslog(LOG_INFO, "player start");
syslog(LOG_INFO, "[player] start");
syslog(LOG_INFO, "player: start");
```

**正确做法**：

```c
// ✅ 正确：统一格式
syslog(LOG_INFO, "[ktv][player] start song_id=%s", id);
syslog(LOG_INFO, "[ktv][net] request endpoint=%s", endpoint);
```

#### ❌ 坑 9：依赖日志恢复业务

日志只用于诊断。

**错误示例**：

```c
// ❌ 错误：从日志恢复状态
void restoreState() {
    FILE* fp = popen("logread | grep state", "r");
    // 解析日志恢复状态  // 禁止！
}
```

**正确做法**：

```c
// ✅ 正确：用数据库/配置文件恢复状态
void restoreState() {
    // 从数据库或配置文件恢复
    ConfigService::instance().load();
}
```

#### ❌ 坑 10：认为 syslog "慢"

99% 的慢是你用错。

**常见误解**：

* "syslog 会阻塞"
* "syslog 性能差"

**事实**：

* syslog 是系统调用，非常快
* 慢的是你的用法（高频、大对象、网络上传）

---

## 🎯 总结

### 架构原则

1. ✅ **syslog 直接调用**：不要包装线程，不要队列
2. ✅ **LogUploadService 独立线程**：低优先级，按需触发
3. ✅ **日志格式统一**：`[ktv][module][event] key=value`
4. ✅ **级别控制**：生产关闭 DEBUG
5. ✅ **上传按需**：只在关键错误时触发

### 系统边界

```
KTV App → syslog() → syslogd → logread → LogUploadService → HTTP
```

**各层职责清晰，互不干扰。**

---

**最后更新**: 2025-12-30  
**状态**: ✅ 核心文档（合并版，包含日志子系统设计、规范、服务设计、实现示例）



