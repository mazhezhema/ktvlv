# LogUploadService 设计（F133 / Tina）

> **文档版本**：v1.0  
> **最后更新**：2025-12-30  
> **状态**：✅ 核心文档  
> **适用平台**：F133 / Tina Linux（BusyBox 体系）

---

## 1. 目标

- 不影响 UI/播放（低优先级、低频）
- 支持"按需抓取 + 批量上传"
- 支持离线缓存（可选）与网络恢复补传（可选）
- 支持一键导出（用户反馈/售后）

---

## 2. 组件与线程

### 2.1 线程划分

- **主线程**：UI/LVGL loop（禁止日志抓取/上传）
- **Player 线程**：播放控制（只打 syslog，不做上传）
- **Network 线程**：HTTP 请求（只打 syslog，不做上传）
- **LogUpload 线程（新增，低优先级）**
  - 任务：抓取 logread 输出、过滤、打包、上传
  - 唤醒：事件触发 + 定时器（低频）

### 2.2 交互方式

- 各模块不直接调用 logread
- 各模块只"发信号"给 LogUploadService：
  - push 一个 UploadReason（枚举）
  - 或设置一个"需要上传"的标记位

**推荐**：`std::queue + std::mutex + condition_variable`（MVP 够用）

---

## 3. 状态机

### 状态定义

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

### 状态转换图

```
[IDLE]  ←→  [COLLECTING]  →  [PACKING]  →  [UPLOADING]  →  [IDLE]
  ↑                                                              ↓
  └────────── [BACKOFF] ←──────────────────────────────────────┘
```

---

## 4. 触发策略（建议）

### 触发源（谁发任务）

- **PlayerService**：
  - 播放失败（ERR）
  - 连续 3 次解码失败
- **NetworkService**：
  - 连续 N 次 http_fail
- **UI**：
  - 用户点击"反馈/导出日志"
- **系统**：
  - 每 X 小时低频上传一次摘要（可选，不建议 MVP 开）

### 触发合并策略

- 10 分钟内多次触发 → 合并为一次上传（避免刷屏/刷网）

---

## 5. 数据上限保护（必须）

- 单次抓取最大行数：例如 2000 行
- 单次抓取最大字节：例如 256KB（硬上限）
- 单次上传最大包体：例如 512KB
- 超出则截断，并记录：
  - `[ktv][log][truncate] bytes=... lines=...`

---

## 6. 脱敏规则（必须）

上传前必须做：

- 替换 token / auth header / cookie
- URL 去 query（保留 path）
- 设备序列号可 hash

---

## 7. 最简接口（给上层调用）

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

### 上层调用者

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

## 8. 实现要点

### 8.1 线程模型

- 独立后台线程（低优先级）
- 使用 `std::queue<UploadReason> + std::mutex + std::condition_variable`
- 线程主循环：等待触发 → 收集日志 → 打包 → 上传 → 回到等待

### 8.2 日志收集

```cpp
bool CollectKtvLogs(std::string& out, const LogCollectConfig& cfg) {
    FILE* fp = popen("logread", "r");
    if (!fp) return false;
    
    char line[512];
    size_t lines = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (cfg.include_keyword && std::strstr(line, cfg.include_keyword) == nullptr) {
            continue;
        }
        
        // 检查上限
        if (out.size() + strlen(line) > cfg.max_bytes) {
            out.append("[ktv][log][truncate] reason=max_bytes\n");
            break;
        }
        
        out.append(line);
        lines++;
        if (lines >= cfg.max_lines) {
            out.append("[ktv][log][truncate] reason=max_lines\n");
            break;
        }
    }
    
    pclose(fp);
    return true;
}
```

### 8.3 上传实现

- 使用 libcurl（复用 HttpService）
- 超时：3~5s
- 重试：最多 2 次
- 失败进入 backoff

---

## 9. 与现有架构的集成

### 9.1 与 HttpService 的关系

```cpp
// LogUploadService 可以复用 HttpService
bool LogUploadService::uploadLogs(const std::string& payload) {
    return HttpService::instance().post("/api/logs/upload", payload);
}
```

### 9.2 与线程架构的关系

- LogUploadService 是独立的服务线程
- 不干扰 UI 线程、Player 线程、Network 线程
- 使用 `std::queue + mutex`（与项目架构一致）

---

## 10. MVP 实现优先级

### Phase 1（必须）

- [x] Notify(reason) 接口
- [x] 线程主循环
- [x] logread 抓取 + 过滤
- [x] 数据上限保护
- [x] 基本上传（POST）

### Phase 2（可选）

- [ ] 离线缓存
- [ ] 网络恢复补传
- [ ] 一键导出到文件
- [ ] gzip 压缩

---

**最后更新**: 2025-12-30  
**状态**: ✅ 核心文档，LogUploadService设计

