# 历史记录 SQLite 实现设计（F133 / Tina Linux）

> **文档版本**：v1.0  
> **最后更新**：2025-12-30  
> **状态**：✅ 核心文档  
> **适用平台**：F133 / Tina Linux（SQLite 系统自带）  
> **数据量级**：50～100 条历史记录

---

## 🎯 一句话结论

> **50 / 100 条历史记录 = SQLite 的"甜蜜区间"**  
> ✔ 性能不是问题  
> ✔ WAL/事务都可以极简  
> ✔ 不需要复杂索引  
> ✔ DB 文件永远很小（KB 级）  
> ✔ 断电/刷机风险极低  
> ✔ 可直接用于问题复现、日志上报

---

## 📋 目录

1. [设计概述](#一-设计概述)
2. [数据库设计](#二-数据库设计)
3. [线程模型](#三-线程模型)
4. [实现细节](#四-实现细节)
5. [最佳实践](#五-最佳实践)
6. [与现有架构的集成](#六-与现有架构的集成)

---

## 一、设计概述

### 1.1 为什么使用 SQLite？

**数据量级**：50～100 条历史记录

**选择 SQLite 的理由**：
- ✅ F133 系统内置 SQLite（无需额外依赖）
- ✅ 后续可能扩字段、扩行为（播放次数、最近一次播放位置、来源、命中策略）
- ✅ 希望**稳、可扩展、可查询**
- ✅ 在这个量级下，SQLite 几乎"零成本"

**为什么不直接用 JSON 文件？**
- JSON 文件也能干，但 SQLite 更稳、可扩展、可查询
- 系统已内置 SQLite，无需额外依赖

### 1.2 核心设计原则

1. **Singleton 模式**：HistoryDbService 单例
2. **使用 SqliteHelper**：进程唯一 DB，统一入口
3. **50/100 条上限**：每次插入后立即裁剪
4. **返回值表示状态**：所有函数返回 `int`（0 成功，<0 失败）
5. **WAL 模式**：轻量级配置，性能足够
6. **极简设计**：避免过度封装，不搞 ORM

---

## 二、数据库设计

### 2.1 表结构

```sql
CREATE TABLE IF NOT EXISTS history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    song_id TEXT NOT NULL,
    song_name TEXT,
    artist TEXT,
    local_path TEXT,
    played_at INTEGER NOT NULL  -- unix timestamp
);
```

**字段说明**：
- `id`：自增主键
- `song_id`：歌曲ID（必填）
- `song_name`：歌曲名称
- `artist`：歌手
- `local_path`：本地文件路径（可选）
- `played_at`：播放时间（unix timestamp）

### 2.2 索引策略

**不创建索引**（50/100 条数据量级）：
- 100 条以内，全表扫描 < 1ms
- 少一个 index = 少一次 flash 写入
- 真正优化的是**写路径**，不是查路径

> ❌ 不建议现在就加 index  
> ✔ 真要加，只加一个 `played_at`（如果后续需要按时间范围查询）

### 2.3 强制上限策略

**每次插入后，立刻裁剪**：

```sql
-- 插入
INSERT INTO history (song_id, song_name, artist, local_path, played_at)
VALUES (?, ?, ?, ?, ?);

-- 保留最近 N 条（50 或 100）
DELETE FROM history
WHERE id NOT IN (
    SELECT id FROM history
    ORDER BY played_at DESC
    LIMIT 100
);
```

**为什么这比"定时清理"好？**
- 逻辑简单
- 不依赖定时器
- 不怕异常重启
- DB 永远小

> 100 条数据，这个 DELETE 的代价是**可以忽略的**

---

## 三、线程模型

### 3.1 推荐模型（与项目架构完全匹配）

```
LVGL/UI 线程
   |
   |  HistoryEvent（通过消息队列）
   v
std::queue + mutex
   |
   v
业务线程（或独立 DB 线程）
   |
   v
HistoryDbService
   |
   v
SQLite
```

### 3.2 为什么不让 UI 线程直连 SQLite？

- LVGL 主线程一旦被 I/O 卡住，体验直接翻车
- DB 写是不可预测延迟（flash / fsync）
- 多线程 SQLite 出问题时排查成本极高

### 3.3 实现方式

**方案 A：业务线程中同步操作**（推荐 MVP）
- 在业务线程中直接调用 `HistoryDbService::addRecord()`
- 简单直接，符合项目架构
- 50/100 条数据，同步操作完全可接受

**方案 B：独立 DB 线程**（可选，如果业务线程已很忙）
- 创建独立的 DB 线程
- 通过消息队列接收任务
- 更解耦，但增加复杂度

**MVP 建议**：使用方案 A（业务线程中同步操作）

---

## 四、实现细节

### 4.1 PRAGMA 配置（轻量级）

```sql
PRAGMA journal_mode=WAL;
PRAGMA synchronous=NORMAL;
PRAGMA temp_store=MEMORY;
PRAGMA cache_size=-512;   -- 512KB 足够
```

**说明**：
- `WAL`：Write-Ahead Logging，性能更好
- `synchronous=NORMAL`：平衡性能和安全性
- `temp_store=MEMORY`：临时数据放内存
- `cache_size=-512`：512KB 缓存（足够 50/100 条数据）

### 4.2 Prepared Statement（必须使用）

**必须做**：
- 用 `sqlite3_prepare_v2`
- 用 `sqlite3_bind_*`
- 不要拼 SQL 字符串

**不要做**：
- 不要搞"通用 ORM"
- 不要搞"JSON ↔ SQLite 自动映射"
- 不要搞"模板元编程"

**你们这是 KTV，不是金融系统。**

### 4.3 事务与写频率

**正确姿势（历史记录场景）**：
- **一首歌结束时写一次**
- **一次写 = 一个事务**
- 不要边播边写（没意义）

```sql
BEGIN;
INSERT ...
DELETE ...  -- 裁剪到 max_count
COMMIT;
```

👉 在 100 条规模下：
- 每首歌 1 次 fsync
- 完全可接受
- 用户感知不到

### 4.4 错误处理

**必须处理**：
- 数据库打开失败
- 插入失败
- 裁剪失败（警告即可，不影响插入）

**日志记录**：
- 使用 syslog 记录所有错误
- 格式：`[ktv][db][error] component=history_db action=xxx reason=xxx`

---

## 五、SQLite 使用规则（禁止事项）

> **⚠️ 铁律**：SQLite 使用规则必须严格遵守，违反这些规则会导致数据损坏、性能问题或系统崩溃。

### ❌ 严格禁止事项

1. **禁止多线程写数据库**
   - ❌ 多个线程同时执行 INSERT/UPDATE/DELETE
   - ✅ 所有数据库操作必须在单一线程中执行（通过消息队列）

2. **禁止 UI 线程直接执行 SQL**
   - ❌ 在 UI 回调中直接调用 `sqlite3_exec()`
   - ❌ 在 LVGL 事件处理中直接操作数据库
   - ✅ 所有数据库操作通过 Service 层（消息队列）

3. **禁止长事务**
   - ❌ 事务跨越多个业务操作
   - ❌ 事务中包含耗时操作（网络请求、文件IO）
   - ✅ 事务范围最小化，只包含数据库操作

4. **禁止使用 ORM**
   - ❌ 使用 ORM 框架（SQLiteORM、sqlpp11 等）
   - ✅ 直接使用 SQLite C API 和 Prepared Statement

5. **禁止动态 SQL 拼接**
   - ❌ `sprintf(sql, "SELECT * FROM songs WHERE id=%d", id)`（SQL注入风险）
   - ✅ 使用 Prepared Statement 和参数绑定

6. **禁止在循环中执行 SQL**
   - ❌ `for (auto& item : items) { sqlite3_exec(db, "INSERT ..."); }`
   - ✅ 使用事务批量插入：`BEGIN; INSERT ...; INSERT ...; COMMIT;`

7. **禁止忽略错误处理**
   - ❌ 不检查 `sqlite3_exec()` 返回值
   - ✅ 所有 SQL 操作必须检查返回值并处理错误

### ✅ 推荐做法

1. **使用 Prepared Statement**
   ```c
   // ✅ 正确：使用 Prepared Statement
   sqlite3_stmt* stmt;
   sqlite3_prepare_v2(db, "INSERT INTO history (song_id, title) VALUES (?, ?)", -1, &stmt, NULL);
   sqlite3_bind_text(stmt, 1, song_id, -1, SQLITE_STATIC);
   sqlite3_bind_text(stmt, 2, title, -1, SQLITE_STATIC);
   sqlite3_step(stmt);
   sqlite3_finalize(stmt);
   ```

2. **使用事务批量操作**
   ```c
   // ✅ 正确：使用事务批量插入
   sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);
   for (auto& item : items) {
       // 插入操作
   }
   sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
   ```

3. **单线程访问（通过消息队列）**
   ```c
   // ✅ 正确：所有数据库操作通过消息队列
   void add_history_item(const HistoryItem& item) {
       // 通过消息队列发送到数据库线程
       DatabaseCmd cmd;
       cmd.type = DB_CMD_INSERT;
       cmd.data = &item;
       DatabaseCmdQueue::instance().enqueue(cmd);
   }
   ```

4. **错误处理和日志**
   ```c
   // ✅ 正确：检查错误并记录日志
   int rc = sqlite3_step(stmt);
   if (rc != SQLITE_DONE) {
       syslog(LOG_ERR, "[ktv][db] SQL error: %s", sqlite3_errmsg(db));
       return -1;
   }
   ```

### 详细说明

**多线程写数据库的风险**：
- SQLite 虽然支持多线程，但在嵌入式环境中，多线程写会导致锁竞争和性能下降
- 更严重的是，如果多个线程同时写，可能导致数据损坏

**UI 线程直接执行 SQL 的风险**：
- UI 线程阻塞会导致界面卡顿
- LVGL 事件处理中执行 SQL 可能触发死锁

**长事务的风险**：
- 长事务会持有数据库锁，阻塞其他操作
- 事务中包含耗时操作会导致数据库长时间不可用

---

## 六、最佳实践

### 5.1 初始化流程

```cpp
// 应用启动时（返回 0 成功，<0 失败）
int initHistoryDb() {
    return HistoryDbService::instance().initialize(
        "/data/ktv_history.db",  // 数据库路径
        50  // 最大记录数
    );
}
```

### 5.2 添加记录流程

```cpp
// 播放结束时（返回 0 成功，<0 失败）
int onSongFinished(const std::string& song_id, 
                   const std::string& song_name,
                   const std::string& artist,
                   const std::string& local_path) {
    // 在业务线程中调用
    return HistoryDbService::instance().addRecord(
        song_id, song_name, artist, local_path
    );
}
```

### 5.3 查询记录流程

```cpp
// UI 线程需要显示历史记录时
int loadHistoryList() {
    // 在业务线程中查询
    std::vector<HistoryDbItem> items;
    int ret = HistoryDbService::instance().getHistoryList(items, 50);
    if (ret != 0) return ret;
    
    // 通过消息队列发送到 UI 线程
    UiEventQueue::push(HistoryListEvent{items});
    return 0;
}
```

### 5.4 DB 文件大小 & 运维角度

在这个设计下：
- **DB 文件大小**：几十 KB 级别
- **WAL 文件**：基本不增长
- **复制 / 上传 / 导出**：可以直接拷文件

这对后续做：
- 远程问题复现
- 客服排查
- 日志 + DB 一起上传

是**极大利好**。

---

## 六、与现有架构的集成

### 6.1 与 HistoryService 的关系

**方案 A：HistoryService 内部使用 HistoryDbService**（推荐）
- `HistoryService` 作为业务层接口
- 内部使用 `HistoryDbService` 进行持久化
- 保持现有 API 不变

**方案 B：直接使用 HistoryDbService**（可选）
- 业务层直接调用 `HistoryDbService`
- 更直接，但需要修改现有代码

**MVP 建议**：使用方案 A（保持现有 API 不变）

### 6.2 与消息队列的关系

**历史记录操作流程**：

```
UI 线程（播放结束）
   |
   |  HistoryEvent（通过 UiEventQueue）
   v
业务线程
   |
   |  HistoryDbService::addRecord()
   v
SQLite
```

**查询历史记录流程**：

```
UI 线程（需要显示历史）
   |
   |  QueryHistoryEvent（通过 UiEventQueue）
   v
业务线程
   |
   |  HistoryDbService::getHistoryList()
   v
SQLite
   |
   |  HistoryListEvent（通过 UiEventQueue）
   v
UI 线程（更新 UI）
```

### 6.3 与 JSON / cJSON 的关系

> **结论：SQLite 存数据，JSON 传数据**

- **SQLite**：
  - 本地结构化存储
  - 查询 / 去重 / 裁剪
- **JSON / cJSON**：
  - REST API
  - 上传 history 给 server
  - 下载配置

❌ **不建议**：
- SQLite 里塞 JSON blob
- JSON 文件当数据库用（已经有 SQLite）

---

## 七、实现检查清单

### ✅ 必须实现（MVP阶段）

- [ ] Singleton 模式
- [ ] 数据库初始化（创建表、配置 PRAGMA）
- [ ] Prepared Statement（INSERT、SELECT、DELETE、COUNT、CLEAR）
- [ ] 每次插入后立即裁剪到 max_count
- [ ] 错误处理和日志记录
- [ ] 关闭数据库时释放资源

### ⚠️ 禁止事项

- [ ] ❌ 不要在 UI 线程直接调用 SQLite
- [ ] ❌ 不要拼 SQL 字符串（必须用 prepared statement）
- [ ] ❌ 不要创建不必要的索引
- [ ] ❌ 不要搞复杂的 ORM 封装

---

## 八、性能考虑

### 8.1 50/100 条数据量级下的性能

| 操作 | 预期耗时 | 说明 |
|------|---------|------|
| 插入 1 条 | < 1ms | 包含裁剪操作 |
| 查询 50 条 | < 1ms | 全表扫描 |
| 裁剪到 50 条 | < 1ms | DELETE 操作 |
| 清空所有 | < 1ms | DELETE 操作 |

👉 **在这个量级下，性能不是问题**

### 8.2 写频率

- **一首歌结束时写一次**
- **一次写 = 一个事务**
- 不要边播边写（没意义）

---

## 九、故障处理

### 9.1 数据库损坏

**处理方式**：
- 检测到损坏时，清空数据库
- 记录错误日志
- 不影响播放功能

### 9.2 磁盘空间不足

**处理方式**：
- 检测到写入失败时，记录错误日志
- 不影响播放功能
- 可以降级为内存模式（可选）

### 9.3 断电风险

**处理方式**：
- WAL 模式 + `synchronous=NORMAL` 已足够安全
- 50/100 条数据，即使丢失 1 条，业务上完全可接受

---

## 十、后续扩展（可选）

### 10.1 可能的扩展字段

- `play_count`：播放次数
- `last_position`：最近一次播放位置
- `source`：来源（点歌/搜索/推荐）
- `favorite`：是否收藏

### 10.2 可能的扩展功能

- 按时间范围查询
- 按歌手查询
- 去重统计
- 播放次数排序

**但 MVP 阶段不需要这些功能**

---

## 📚 相关文档

- **服务层API设计**：[服务层API设计文档.md](../服务层API设计文档.md)
- **线程架构基线**：[线程架构基线（最终版）.md](../线程架构基线（最终版）.md)
- **资源管理规范**：[资源管理规范v1.md](../资源管理规范v1.md)
- **F133平台库清单**：[F133平台库清单.md](../F133平台库清单.md)

---

**最后更新**: 2025-12-30  
**状态**: ✅ 核心文档，历史记录 SQLite 实现设计

