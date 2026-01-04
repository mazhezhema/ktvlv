# KTV 日志规范（F133 / Tina Linux / syslog）

> **文档版本**：v1.0  
> **最后更新**：2025-12-30  
> **状态**：✅ 核心文档  
> **目标**：量产可排障、低开销、可 grep、可上传、不中断 UI/播放  
> **适用范围**：F133 + Tina Linux + LVGL KTV App（C/C++）

---

## 1. 核心原则

1. **syslog 是唯一正式日志入口**：所有模块统一走 syslog()
2. **日志"事件化"，不"过程化"**：记录状态迁移、错误、关键动作
3. **禁止在实时路径打日志**：UI tick / 音频回调 / 播放回调严禁 syslog
4. **生产版本关闭 DEBUG**：仅保留 INFO/WARN/ERR（按策略）
5. **日志不承担业务**：日志失败不影响业务流程
6. **默认不落盘**：避免 Flash/eMMC 写放大；必要时仅"按需导出"

---

## 2. 日志格式标准

### 2.1 统一 Tag

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

### 2.2 必须使用 key=value

- 便于 grep / 解析 / 上传结构化
- 允许用短 key：`song_id`, `code`, `err`, `page`, `ms`

---

## 3. 日志级别约定

| syslog level | 使用场景 | 是否建议上传 |
|-------------|---------|------------|
| **LOG_ERR** | 播放失败、解码失败、关键资源加载失败、崩溃前关键点 | 必传 |
| **LOG_WARNING** | 网络慢/超时重试、资源不足、降级策略触发 | 条件传 |
| **LOG_INFO** | 状态变化、关键业务动作、启动阶段里程碑 | 条件传 |
| **LOG_DEBUG** | 仅开发调试；生产关闭 | 不传 |

---

## 4. 哪些线程/回调禁止打日志（红线）

### ❌ 禁止 syslog 的场景

- LVGL tick / lv_timer 回调
- UI 频繁事件（滑动、动画帧）
- 音频回调线程
- 播放器高频回调（progress / frame ready）
- 任何 while(1) 高频循环

### ✅ 允许 syslog 的场景（低频）

- 状态切换（Idle -> Loading -> Playing -> Error）
- 播放开始/结束/失败
- HTTP 请求失败/重试/成功摘要
- 存储加载/保存结果
- 用户关键动作（点歌、切歌、收藏）

---

## 5. 各模块必打日志点（建议清单）

### 5.1 App 生命周期

```
[start] app_start version=...
[ready] lvgl_ok player_ok net_ok
[exit] app_exit reason=...
```

### 5.2 Player

```
[start] song_id=... bitrate=... track=...
[switch] from=... to=...
[fail] song_id=... err=... stage=open/decode/buffer
[finish] song_id=... ms=...
```

### 5.3 Network

```
[req] method=GET path=... rid=...
[http_fail] rid=... code=... retry=... ms=...
[ok] rid=... code=200 ms=... bytes=...
```

### 5.4 Storage（历史/收藏）

```
[history_load_ok] count=...
[history_load_fail] err=...
[history_save_ok] count=...
[history_save_fail] err=...
```

### 5.5 UI（只允许低频事件）

```
[page_enter] page=...
[action] name=play_click song_id=...
```

---

## 6. 日志封装宏规范（建议）

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

---

## 7. 上传规范（简要）

- 上传只抓取 `[ktv]` 前缀日志
- 上传触发：播放失败/连续网络错误/用户反馈/运维指令
- 上传内容脱敏：禁止 token、完整 URL query、用户隐私字段

**详见**：[日志子系统设计说明.md](./日志子系统设计说明.md)

---

## 8. Code Review Checklist（必须检查）

- [ ] 是否在 UI tick / 音频回调打日志？（必须否）
- [ ] 是否用 `[ktv][module][event]` 格式？（必须是）
- [ ] 是否带 key=value？（必须是）
- [ ] 是否遗漏 coredump/关键失败日志点？（必须补）
- [ ] 是否在生产版本输出 DEBUG？（必须否）
- [ ] 是否包含隐私/密钥？（必须否）

---

## 9. 示例代码

### 9.1 正确示例

```c
// ✅ 正确：状态切换时打日志
void PlayerAdapter::onStateChange(PlayerState from, PlayerState to) {
    syslog(LOG_INFO, "[ktv][player][state_change] from=%d to=%d", from, to);
}

// ✅ 正确：播放失败时打日志
void PlayerAdapter::onError(int code) {
    syslog(LOG_ERR, "[ktv][player][fail] err=%d", code);
    LogUploadService::instance().triggerUpload();
}

// ✅ 正确：HTTP请求摘要
void HttpService::onResponse(int code, const char* path) {
    syslog(LOG_INFO, "[ktv][net][ok] path=%s code=%d", path, code);
}
```

### 9.2 错误示例

```c
// ❌ 错误：在UI tick中打日志
void ui_tick() {
    syslog(LOG_DEBUG, "[ktv][ui] tick");  // 禁止！
}

// ❌ 错误：在音频回调中打日志
void audio_callback(void* data, int len) {
    syslog(LOG_DEBUG, "[ktv][audio] len=%d", len);  // 禁止！
}

// ❌ 错误：格式不统一
syslog(LOG_INFO, "player start");  // 禁止！必须用 [ktv][player][start]
```

---

**最后更新**: 2025-12-30  
**状态**: ✅ 核心文档，KTV日志规范


