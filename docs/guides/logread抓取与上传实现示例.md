# logread 抓取与上传实现示例（C++ / Shell）

> **文档版本**：v1.0  
> **最后更新**：2025-12-30  
> **状态**：✅ 核心文档  
> **适用平台**：F133 / Tina Linux（BusyBox 体系）

---

## 1. 概述

本文档提供两套实现方案：

- **(A) C++ 内置实现**（更工程化，推荐 MVP）
- **(B) shell 导出 + C++ 调用**（更运维友好，售后工具）

**MVP 建议**：先上 A，B 作为售后工具补充。

---

## 2. 方案 A：C++ 内置实现

### 2.1 日志收集函数

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

---

### 2.2 JSON 打包函数

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

---

### 2.3 上传函数（骨架）

#### log_upload.cpp（示意骨架）

```cpp
// log_upload.cpp (skeleton)
#include <string>

// TODO: 你们已有 libcurl 封装就复用，这里只示意职责边界
bool UploadLogsToServer(const std::string& payload_json) {
    // POST /api/logs/upload
    // 超时：3~5s
    // 重试：最多 2 次
    // 失败进入 backoff
    
    // 示例：使用 HttpService
    // return HttpService::instance().post("/api/logs/upload", payload_json);
    
    return true;
}
```

---

## 3. 方案 B：shell 导出 + C++ 调用（售后工具）

### 3.1 shell 脚本

#### export_ktv_log.sh

```sh
#!/bin/sh
# 导出最近的 KTV 相关日志到 /tmp（默认 RAM盘）
OUT=/tmp/ktv.log
logread | grep "\[ktv\]" > "$OUT"
echo "$OUT"
```

**赋权**：

```sh
chmod +x /usr/bin/export_ktv_log.sh
```

---

### 3.2 C++ 调用脚本

```cpp
// log_export.cpp
#include <cstdio>
#include <string>

/**
 * 通过 shell 脚本导出日志
 * @return 日志文件路径，失败返回空字符串
 */
std::string ExportLogViaShell() {
    FILE* fp = popen("/usr/bin/export_ktv_log.sh", "r");
    if (!fp) {
        return "";
    }
    
    char path[256] = {0};
    if (fgets(path, sizeof(path), fp) == nullptr) {
        pclose(fp);
        return "";
    }
    
    pclose(fp);
    
    // 去掉换行
    std::string p(path);
    while (!p.empty() && (p.back() == '\n' || p.back() == '\r')) {
        p.pop_back();
    }
    
    return p;
}
```

---

## 4. 完整使用示例

### 4.1 收集 + 打包 + 上传

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

## 5. 注意事项

### 5.1 数据上限保护

- 必须检查 `max_bytes` 和 `max_lines`
- 超出时截断并记录 `[ktv][log][truncate]`

### 5.2 错误处理

- `popen()` 失败：返回 false
- `pclose()` 返回值非 0：返回 false
- 上传失败：记录日志，进入 backoff

### 5.3 脱敏处理

上传前必须脱敏：

```cpp
// 示例：脱敏函数
std::string SanitizeLogs(const std::string& logs) {
    std::string out = logs;
    
    // 替换 token
    // TODO: 实现 token 替换逻辑
    
    // URL 去 query
    // TODO: 实现 URL 清理逻辑
    
    return out;
}
```

---

## 6. 与 LogUploadService 的集成

这些函数应该被 `LogUploadService` 调用：

```cpp
// LogUploadService::threadLoop() 中
void LogUploadService::threadLoop() {
    while (running_) {
        // 等待触发...
        
        // 收集日志
        std::string logs;
        if (!CollectKtvLogs(logs, cfg_)) {
            continue;
        }
        
        // 打包
        std::string payload = BuildUploadPayloadJson(
            device_id_, fw_version_, uptime_sec_, logs);
        
        // 上传
        if (!UploadLogsToServer(payload)) {
            // 进入 backoff
            enterBackoff();
        }
    }
}
```

---

**最后更新**: 2025-12-30  
**状态**: ✅ 核心文档，logread抓取与上传实现示例


