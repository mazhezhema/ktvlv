// log_macros.h
// 日志语法糖：统一格式 [ktv][component][level]
#pragma once

#include <syslog.h>

/**
 * 日志宏 - 统一格式
 *
 * 格式：[ktv][component][level] message
 *
 * 用法：
 *   KTV_LOG_INFO("db", "action=init path=%s", path);
 *   KTV_LOG_ERR("http", "action=get reason=timeout url=%s", url);
 *   KTV_LOG_WARN("player", "action=seek position=%d", pos);
 *   KTV_LOG_DEBUG("ui", "action=refresh page=%d", page);
 *
 * 输出：
 *   [ktv][db][info] action=init path=/data/history.db
 *   [ktv][http][error] action=get reason=timeout url=/api/songs
 */

// 基础日志宏
#define KTV_LOG_DEBUG(component, fmt, ...) \
    syslog(LOG_DEBUG, "[ktv][" component "][debug] " fmt, ##__VA_ARGS__)

#define KTV_LOG_INFO(component, fmt, ...) \
    syslog(LOG_INFO, "[ktv][" component "][info] " fmt, ##__VA_ARGS__)

#define KTV_LOG_WARN(component, fmt, ...) \
    syslog(LOG_WARNING, "[ktv][" component "][warn] " fmt, ##__VA_ARGS__)

#define KTV_LOG_ERR(component, fmt, ...) \
    syslog(LOG_ERR, "[ktv][" component "][error] " fmt, ##__VA_ARGS__)

// 带 action 的快捷宏（最常用场景）
#define KTV_LOG_ACTION(component, action, fmt, ...) \
    syslog(LOG_INFO, "[ktv][" component "][action] action=" action " " fmt, ##__VA_ARGS__)

#define KTV_LOG_ACTION_ERR(component, action, reason) \
    syslog(LOG_ERR, "[ktv][" component "][error] action=" action " reason=" reason)

/**
 * 组件名约定（建议统一使用）：
 *
 * | 组件 | 名称 |
 * |------|------|
 * | HttpService | "http" |
 * | SongService | "song" |
 * | PlayerService | "player" |
 * | HistoryService | "history" |
 * | HistoryDbService | "db" |
 * | LicenceService | "licence" |
 * | M3u8DownloadService | "download" |
 * | EventBus | "event" |
 * | PageManager | "ui" |
 * | JsonHelper | "json" |
 */

