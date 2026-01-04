# KTV App 稳定性与自愈设计说明

> **文档版本**：v1.0  
> **最后更新**：2025-12-30  
> **状态**：✅ 核心文档（量产必读）  
> **适用平台**：F133 / Tina Linux  
> **目标**：确保应用在量产环境中的稳定性和可维护性

---

## 🎯 一句话总评

> **架构方向正确，选型成熟保守，但必须补充进程级自愈、线程生命周期、日志远程上传、升级回滚、配置边界和异常UI，否则量产必翻车。**

---

## 📋 目录

1. [进程级 Watchdog / 自愈机制](#一-进程级-watchdog--自愈机制)
2. [线程生命周期 & 退出顺序](#二-线程生命周期--退出顺序)
3. [日志 → 远程上传闭环](#三-日志--远程上传闭环)
4. [应用升级的回滚策略](#四-应用升级的回滚策略)
5. [配置中心的只读/可写边界](#五-配置中心的只读可写边界)
6. [异常兜底 UI](#六-异常兜底-ui)
7. [工程稳定性检查清单](#七-工程稳定性检查清单)

---

## 一、进程级 Watchdog / 自愈机制

### 1.1 问题描述

**当前状态**：
- ✅ 有线程架构设计
- ✅ 有事件系统设计
- ✅ 有 Service 设计
- ❌ **缺少 App 级自愈策略**

**风险**：
- 网络抖动 → App 假死
- TPlayer 卡死 → 只能断电重启
- LVGL 线程假死 → UI 无响应
- 内存泄漏累积 → 系统崩溃

### 1.2 自愈机制设计

#### 最低标准（必须实现）

**1. 主进程心跳机制**

```c
// 主进程心跳（每 1~5 秒）
static void heartbeat_check(void) {
    static time_t last_heartbeat = 0;
    time_t now = time(NULL);
    
    if (now - last_heartbeat > HEARTBEAT_TIMEOUT) {
        syslog(LOG_ERR, "[ktv][watchdog] heartbeat timeout, restarting...");
        exit(EXIT_FAILURE);  // 触发 systemd 重启
    }
    
    last_heartbeat = now;
}
```

**2. 子模块状态位检查**

```c
// 三个核心模块状态位
typedef struct {
    bool ui_alive;      // UI/LVGL 线程状态
    bool player_alive;  // Player 线程状态
    bool network_alive; // Network 线程状态
    time_t last_update; // 最后更新时间
} module_status_t;

// 状态检查
static bool check_module_status(module_status_t* status) {
    time_t now = time(NULL);
    
    // 检查各模块是否超过阈值未更新
    if (now - status->last_update > MODULE_TIMEOUT) {
        if (!status->ui_alive || !status->player_alive || !status->network_alive) {
            return false;
        }
    }
    
    return true;
}
```

**3. 连续异常检测与自杀重启**

```c
// 异常计数
static int failure_count = 0;
static const int MAX_FAILURES = 3;

static void handle_module_failure(void) {
    failure_count++;
    
    syslog(LOG_ERR, "[ktv][watchdog] module failure, count=%d", failure_count);
    
    if (failure_count >= MAX_FAILURES) {
        syslog(LOG_CRIT, "[ktv][watchdog] max failures reached, restarting...");
        
        // 清理资源
        cleanup_resources();
        
        // 自杀，让 systemd 重启
        exit(EXIT_FAILURE);
    }
}
```

#### systemd 配置（外部保障）

```ini
[Unit]
Description=KTV Application
After=network.target

[Service]
Type=simple
ExecStart=/opt/app/current/ktvlv
Restart=always
RestartSec=5
StartLimitInterval=60
StartLimitBurst=3

[Install]
WantedBy=multi-user.target
```

### 1.3 实现建议

1. **主循环心跳**：在主循环中每 1 秒检查一次
2. **线程状态报告**：每个线程定期更新状态位
3. **异常阈值**：连续 3 次异常 → 重启
4. **日志记录**：所有异常必须记录到 syslog

---

## 二、线程生命周期 & 退出顺序

### 2.1 问题描述

**当前风险**：
- 线程创建/退出时机不明确
- 退出顺序混乱导致资源泄漏
- 线程重启策略未定义

### 2.2 线程生命周期表

| 线程 | 创建时机 | 退出时机 | 是否可重启 | 退出顺序 | 说明 |
|------|---------|---------|-----------|---------|------|
| **UI/LVGL** | App 启动 | App 退出 | ❌ 否 | 最后退出 | 主线程，管理所有 UI 资源 |
| **Event Loop** | App 启动 | App 退出 | ❌ 否 | 倒数第二 | 事件分发，必须在 UI 之前退出 |
| **Network Worker** | 启动后立即创建 | 网络异常可重启 | ✅ 是 | 第 3 | 网络请求处理，可独立重启 |
| **Player Worker** | 点歌时创建 | 播放结束或停止 | ✅ 是 | 第 2 | 播放器线程，每次播放创建新线程 |
| **LogUpload** | 启动后创建 | App 退出 | ✅ 是（低优先级） | 第 1 | 日志上传，低优先级，可忽略退出 |

### 2.3 退出顺序设计

**关键原则**：
1. **依赖关系决定退出顺序**：被依赖的线程先退出
2. **资源清理顺序**：先清理业务资源，再清理系统资源
3. **优雅退出**：设置退出标志，等待线程自然退出（超时强制退出）

**退出流程**：

```
1. 设置全局退出标志 (g_app_quit = true)
2. 停止所有 Worker 线程（Network、Player、LogUpload）
3. 停止 Event Loop（等待事件处理完成）
4. 停止 UI/LVGL 线程（最后清理 UI 资源）
5. 清理全局资源（Singleton 实例）
6. 退出主进程
```

**代码示例**：

```c
// 全局退出标志
static volatile bool g_app_quit = false;

// 退出顺序控制
static void cleanup_threads(void) {
    // 1. 设置退出标志
    g_app_quit = true;
    
    // 2. 停止 Worker 线程（按顺序）
    log_upload_service_stop();      // LogUpload（第1）
    player_service_stop();           // Player（第2）
    network_service_stop();          // Network（第3）
    
    // 3. 停止 Event Loop（倒数第二）
    event_bus_stop();
    
    // 4. 停止 UI/LVGL（最后）
    ui_system_cleanup();
    
    // 5. 清理全局资源
    cleanup_singletons();
}
```

### 2.4 线程重启策略

**可重启线程的重启规则**：

| 线程 | 重启触发条件 | 重启延迟 | 最大重启次数 |
|------|------------|---------|------------|
| Network Worker | 网络异常、连接断开 | 3 秒 | 5 次/小时 |
| Player Worker | 播放失败、TPlayer 异常 | 立即 | 3 次/播放 |
| LogUpload | 上传失败 | 指数退避（10s, 30s, 60s） | 无限制（低优先级） |

---

## 三、日志 → 远程上传闭环

### 3.1 当前状态

**已有**：
- ✅ syslog 日志系统
- ✅ logread 本地读取
- ✅ LogUploadService 设计

**缺失**：
- ❌ 关键日志 ring buffer（内存）
- ❌ 异常触发打包策略
- ❌ HTTP 上传实现（gzip）

### 3.2 日志远程上传设计

#### 1. 关键日志 Ring Buffer

```c
// 关键日志 ring buffer（内存，最近 1000 条）
#define LOG_RING_BUFFER_SIZE 1000

typedef struct {
    char message[256];
    time_t timestamp;
    int level;
} log_entry_t;

static log_entry_t log_ring_buffer[LOG_RING_BUFFER_SIZE];
static int log_ring_index = 0;
static pthread_mutex_t log_ring_mutex = PTHREAD_MUTEX_INITIALIZER;

// 关键日志记录（ERROR 和 WARNING）
void log_critical(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    // 写入 syslog
    syslog(LOG_ERR, "%s", buffer);
    
    // 写入 ring buffer
    pthread_mutex_lock(&log_ring_mutex);
    log_entry_t* entry = &log_ring_buffer[log_ring_index];
    strncpy(entry->message, buffer, sizeof(entry->message) - 1);
    entry->timestamp = time(NULL);
    entry->level = LOG_ERR;
    log_ring_index = (log_ring_index + 1) % LOG_RING_BUFFER_SIZE;
    pthread_mutex_unlock(&log_ring_mutex);
}
```

#### 2. 异常触发打包

**触发条件**：
- 播放失败
- 网络连续错误（3 次）
- 内存不足
- TPlayer 异常
- 用户反馈按钮触发

**打包策略**：
- 最近 10 分钟的 syslog 日志
- Ring buffer 中的关键日志
- 系统信息（uptime、内存、CPU）
- 应用状态（版本、配置）

```c
// 异常触发打包
typedef struct {
    char* logs;           // 日志内容（gzip 压缩）
    size_t logs_size;     // 压缩后大小
    char device_id[64];   // 设备ID
    char version[32];     // 版本号
    time_t timestamp;     // 时间戳
    int error_code;       // 错误码
} log_package_t;

log_package_t* package_logs(int error_code) {
    // 1. 收集 syslog（最近 10 分钟）
    // 2. 收集 ring buffer
    // 3. 收集系统信息
    // 4. gzip 压缩
    // 5. 构建 JSON payload
    // ...
}
```

#### 3. HTTP 上传（gzip）

```c
// 使用 libcurl 上传（gzip 压缩）
int upload_logs(log_package_t* package) {
    CURL* curl = curl_easy_init();
    if (!curl) return -1;
    
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Content-Encoding: gzip");
    
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.example.com/logs/upload");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, package->logs);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, package->logs_size);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    
    return (res == CURLE_OK) ? 0 : -1;
}
```

### 3.3 实现建议

1. **Ring Buffer 大小**：1000 条（约 256KB 内存）
2. **打包触发频率**：同一错误 10 分钟内只打包一次
3. **上传超时**：10 秒
4. **失败重试**：指数退避（10s, 30s, 60s）

---

## 四、应用升级的回滚策略

### 升级失败态：用户可感知策略

> **升级失败时，系统必须保证：**
>
> * 可继续点歌
> * 不进入循环升级
> * UI 给出明确状态

**这是给 PM / 测试 / 运维看的，不是技术实现细节。**

### 升级失败处理流程

1. **检测升级失败**
   - 升级包下载失败
   - 升级包校验失败
   - 升级后启动失败

2. **回滚到上一版本**
   - 自动切换到备份目录
   - 恢复上一版本配置
   - 记录升级失败日志

3. **用户可感知状态**
   - UI 显示升级失败提示
   - 允许用户继续使用当前版本
   - 不自动重试升级（避免循环）

4. **继续点歌功能**
   - 升级失败不影响点歌功能
   - 播放器正常工作
   - 网络功能正常工作

### 实现示例

```cpp
class UpgradeService {
public:
    void handleUpgradeFailure(UpgradeError error) {
        syslog(LOG_ERR, "[ktv][upgrade] Upgrade failed: %d", static_cast<int>(error));

        // 回滚到上一版本
        rollbackToPreviousVersion();

        // 显示用户可感知的状态
        showUpgradeFailureUI(error);

        // 禁止自动重试（避免循环升级）
        disableAutoUpgrade();

        // 记录升级失败日志
        logUpgradeFailure(error);
    }

    void showUpgradeFailureUI(UpgradeError error) {
        // 通过 Event 通知 UI 显示升级失败提示
        EventBus::getInstance().publish(Event{
            EventType::UPGRADE_FAILURE,
            getErrorMessage(error)
        });
    }

    void rollbackToPreviousVersion() {
        // 切换到备份目录
        system("ln -sfn /opt/app/backup /opt/app/current");

        // 恢复上一版本配置
        restorePreviousConfig();
    }
};
```

---

## 四、应用升级的回滚策略（技术实现）

### 4.1 问题描述

**风险**：
- OTA 升级失败 → 现场砖机
- 新版本启动失败 → 无法回退
- 升级过程中断电 → 系统损坏

### 4.2 升级回滚设计

#### 目录结构

```
/opt/app/
├── current -> app_v1.0.0/    # 当前运行版本（软链接）
├── backup -> app_v0.9.0/     # 备份版本（软链接）
├── app_v1.0.0/               # 版本目录
│   ├── ktvlv                 # 可执行文件
│   ├── config.ini            # 配置文件
│   └── res/                  # 资源文件
└── app_v0.9.0/               # 备份版本目录
```

#### 升级流程

```
1. 下载新版本到临时目录（/tmp/app_v1.1.0/）
2. 验证新版本（校验和、权限检查）
3. 备份当前版本（cp -r current backup）
4. 安装新版本（mv /tmp/app_v1.1.0 /opt/app/app_v1.1.0）
5. 切换软链接（ln -sfn app_v1.1.0 current）
6. 启动新版本
7. 健康检查（启动后 30 秒内无崩溃）
8. 成功 → 删除备份；失败 → 回滚
```

#### 回滚策略

```bash
#!/bin/bash
# 回滚脚本

CURRENT_LINK="/opt/app/current"
BACKUP_LINK="/opt/app/backup"

# 1. 停止当前应用
systemctl stop ktvapp

# 2. 切换回备份版本
ln -sfn $(readlink $BACKUP_LINK) $CURRENT_LINK

# 3. 重启应用
systemctl start ktvapp

# 4. 记录回滚日志
echo "$(date): Rollback to $(readlink $CURRENT_LINK)" >> /var/log/ktv_upgrade.log
```

#### 健康检查

```c
// 启动后健康检查（30 秒）
#define HEALTH_CHECK_TIMEOUT 30

static bool health_check(void) {
    time_t start = time(NULL);
    
    while (time(NULL) - start < HEALTH_CHECK_TIMEOUT) {
        // 检查主进程是否存活
        if (kill(getpid(), 0) != 0) {
            return false;  // 进程已死
        }
        
        // 检查关键模块状态
        if (!check_module_status(&g_status)) {
            return false;  // 模块异常
        }
        
        sleep(1);
    }
    
    return true;  // 健康检查通过
}
```

### 4.3 实现建议

1. **版本目录命名**：`app_v{major}.{minor}.{patch}/`
2. **备份保留**：至少保留 1 个备份版本
3. **健康检查时间**：30 秒（可配置）
4. **自动回滚**：健康检查失败自动回滚

---

## 五、配置中心的只读/可写边界

### 5.1 问题描述

**当前状态**：
- 配置文件分散（JSON / INI / SQLite 混用）
- 配置读写边界不明确
- 热更新策略未定义

### 5.2 配置分类

| 配置类型 | 存储位置 | 是否可写 | 热更新 | 重启生效 | 示例 |
|---------|---------|---------|--------|---------|------|
| **编译期配置** | 代码中（宏定义） | ❌ 不可改 | ❌ | N/A | 分辨率、缓冲区大小 |
| **设备级配置** | `/etc/ktv/device.ini` | ✅ 一次写入 | ❌ | ✅ 是 | 设备ID、MAC地址 |
| **运行期配置** | `/opt/app/config/config.ini` | ✅ 可改 | ✅ 是 | ❌ 否 | 服务器地址、音量 |

### 5.3 配置边界规则

#### 1. 编译期配置（不可改）

```c
// lv_conf.h
#define LV_HOR_RES_MAX 1280
#define LV_VER_RES_MAX 720
#define LV_MEM_CUSTOM 1

// 这些配置在编译时确定，运行时不可修改
```

#### 2. 设备级配置（一次写入）

```ini
# /etc/ktv/device.ini（只读，系统初始化时写入）
[Device]
device_id=KTV001234567890
mac_address=AA:BB:CC:DD:EE:FF
factory_date=2025-01-01
```

**规则**：
- 系统初始化时写入
- 应用运行时只读
- 修改需要 root 权限
- 修改后需要重启应用

#### 3. 运行期配置（UI 可改）

```ini
# /opt/app/config/config.ini（可写）
[Server]
api_url=https://api.example.com
timeout=30

[Player]
default_volume=80
default_audio_track=original

[UI]
theme=dark
language=zh_CN
```

**规则**：
- UI 可以修改
- 支持热更新（无需重启）
- 修改后立即生效
- 使用文件锁防止并发写入

### 5.4 热更新实现

```c
// 配置热更新（使用文件锁）
int update_config(const char* key, const char* value) {
    int fd = open("/opt/app/config/config.ini", O_RDWR);
    if (fd < 0) return -1;
    
    // 文件锁
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    
    if (fcntl(fd, F_SETLKW, &lock) < 0) {
        close(fd);
        return -1;
    }
    
    // 更新配置（使用 inih 库）
    // ...
    
    // 释放锁
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);
    
    // 通知配置变更
    config_service_notify_change(key, value);
    
    return 0;
}
```

### 5.5 配置读取优先级

```
1. 运行时配置（/opt/app/config/config.ini）
2. 设备配置（/etc/ktv/device.ini）
3. 编译期配置（代码宏定义）
```

---

## 六、异常兜底 UI

### 6.1 问题描述

**当前风险**：
- UI 假设一切正常
- 网络失败 → 白屏
- 播放失败 → 无提示
- 升级失败 → 无法操作

### 6.2 异常 UI 设计

#### 1. 网络失败页

**触发条件**：
- HTTP 请求连续失败（3 次）
- WebSocket 连接断开
- DNS 解析失败

**UI 设计**：
```
┌─────────────────────────┐
│   ⚠️ 网络连接失败        │
│                         │
│   请检查网络设置        │
│                         │
│   [重试]  [设置]        │
└─────────────────────────┘
```

#### 2. 播放失败页

**触发条件**：
- TPlayer 播放失败
- M3U8 下载失败
- 音轨切换失败

**UI 设计**：
```
┌─────────────────────────┐
│   ❌ 播放失败           │
│                         │
│   歌曲：稻香            │
│   错误：网络超时        │
│                         │
│   [重试]  [下一首]      │
└─────────────────────────┘
```

#### 3. 升级失败页

**触发条件**：
- OTA 升级失败
- 版本校验失败
- 健康检查失败

**UI 设计**：
```
┌─────────────────────────┐
│   ⚠️ 升级失败           │
│                         │
│   已自动回滚到上一版本  │
│                         │
│   [确定]                │
└─────────────────────────┘
```

#### 4. 空数据页

**触发条件**：
- 搜索无结果
- 歌单为空
- 历史记录为空

**UI 设计**：
```
┌─────────────────────────┐
│   📭 暂无数据           │
│                         │
│   请尝试其他搜索条件    │
│                         │
│   [返回]                │
└─────────────────────────┘
```

### 6.3 实现建议

1. **统一异常页面组件**：`ErrorPage` 组件，参数化错误类型和消息
2. **错误码映射**：错误码 → 错误消息 → UI 页面
3. **自动恢复**：网络错误 30 秒后自动重试
4. **用户操作**：提供重试、返回、设置等操作按钮

---

## 七、工程稳定性检查清单

### 7.1 必须实现项

- [ ] 进程级 Watchdog（心跳 + 状态检查）
- [ ] systemd 服务配置（自动重启）
- [ ] 线程生命周期表（创建/退出/重启策略）
- [ ] 优雅退出流程（按顺序清理线程）
- [ ] 日志 Ring Buffer（内存，1000 条）
- [ ] 异常触发打包（HTTP + gzip）
- [ ] 升级回滚策略（双版本目录）
- [ ] 健康检查机制（30 秒）
- [ ] 配置分类（编译期/设备级/运行期）
- [ ] 配置热更新（文件锁）
- [ ] 异常 UI 页面（网络/播放/升级/空数据）

### 7.2 建议实现项

- [ ] 线程状态监控（实时状态上报）
- [ ] 资源使用监控（内存/CPU）
- [ ] 远程诊断接口（HTTP API）
- [ ] 配置备份机制（升级前备份）

### 7.3 代码审查检查点

1. **线程创建**：是否有明确的创建点和退出点？
2. **资源清理**：退出时是否按顺序清理？
3. **异常处理**：是否有 Watchdog 检查？
4. **日志上传**：异常时是否触发上传？
5. **配置更新**：是否使用文件锁？
6. **UI 异常**：是否有兜底页面？

---

## 📚 相关文档

- [线程架构基线（最终版）.md](../线程架构基线（最终版）.md)
- [KTV日志系统完整指南.md](../guides/KTV日志系统完整指南.md)
- [消息队列完整指南.md](../消息队列完整指南.md)
- [资源管理规范v1.md](../资源管理规范v1.md)

---

**最后更新**: 2025-12-30  
**维护者**: 项目团队


