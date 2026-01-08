# Utils 工具类使用指南

> **文档版本**：v1.0  
> **最后更新**：2025-01-08  
> **状态**：✅ 定版  
> **路径**：`src/utils/`

---

## 📁 文件清单

```
src/utils/
├── json_helper.h      ← JSON 解析唯一入口（包含 JsonDocument + JsonHelper）
├── json_helper.cpp
├── out_value.h        ← 基础类型输出参数包装
├── log_macros.h       ← 日志语法糖（统一格式）
├── sqlite_helper.h    ← SQLite 单例封装（进程唯一 DB）
└── sqlite_helper.cpp
```

---

## 一、OutValue（基础类型输出参数）

### 1.1 设计目的

**禁止在函数参数中出现 `int*` / `bool*` / `double*` 等基础类型指针**。

原因：
- `int*` 语义模糊（是输入？输出？状态？）
- 容易和返回值混淆
- 代码审查时无法一眼判断意图

### 1.2 使用方式

```cpp
#include "utils/out_value.h"

// ❌ 禁止
int GetCount(int* out_count);

// ✅ 正确
int GetCount(ktv::utils::OutInt* out_count);
```

### 1.3 可用类型

| 类型 | 说明 |
|------|------|
| `OutInt` | `int` 输出 |
| `OutLong` | `long` 输出 |
| `OutDouble` | `double` 输出 |
| `OutBool` | `bool` 输出 |
| `OutSizeT` | `size_t` 输出 |
| `OutU32` | `unsigned int` 输出 |

### 1.4 示例

```cpp
ktv::utils::OutInt count;
if (JsonHelper::GetArraySize(doc.root(), &count) == 0) {
    printf("数组大小: %d\n", count.value);
}
```

---

## 二、日志宏（语法糖）

### 2.1 设计目的

统一日志格式，减少重复打字。

格式：`[ktv][component][level] message`

### 2.2 可用宏

```cpp
#include "utils/log_macros.h"

KTV_LOG_DEBUG("db", "action=query count=%d", count);
KTV_LOG_INFO("http", "action=get url=%s", url);
KTV_LOG_WARN("player", "action=seek position=%d", pos);
KTV_LOG_ERR("song", "action=parse reason=invalid_json");
```

### 2.3 快捷宏

```cpp
// 带 action 的快捷写法
KTV_LOG_ACTION("db", "init", "path=%s", path);
// 输出：[ktv][db][action] action=init path=/data/history.db

KTV_LOG_ACTION_ERR("http", "get", "timeout");
// 输出：[ktv][http][error] action=get reason=timeout
```

### 2.4 组件名约定

| 组件 | 名称 |
|------|------|
| HttpService | `"http"` |
| SongService | `"song"` |
| PlayerService | `"player"` |
| HistoryService | `"history"` |
| HistoryDbService | `"db"` |
| LicenceService | `"licence"` |
| M3u8DownloadService | `"download"` |
| EventBus | `"event"` |
| PageManager | `"ui"` |
| JsonHelper | `"json"` |

---

## 三、SqliteHelper（SQLite 单例封装）

### 3.1 一句话定位

> **一个"安全打开 + 防踩坑 + 不烧脑"的 SQLite 外壳。**

不是 ORM，不是 Service，不是 Schema 管理器。

### 3.2 设计原则

- **进程级 Singleton**（static 接口，无实例化）
- **Init 只做 open**，不负责建表
- **建表失败 = 启动失败**（在业务初始化流程处理）
- **只提供 Exec / Query**，不做 ORM

### 3.3 使用边界

| 层级 | 是否允许 |
|------|---------|
| Service 层 | ✅ 允许 |
| UI 层 | ❌ 禁止 |
| Player 层 | ❌ 禁止 |
| LVGL callback | ❌ 禁止 |

### 3.4 API（只有 4 个）

```cpp
#include "utils/sqlite_helper.h"
using ktv::utils::SqliteHelper;
using ktv::utils::SqlRow;

// 初始化（启动时调用一次）
SqliteHelper::Init("/data/ktv.db");

// 执行（INSERT / UPDATE / DELETE / CREATE TABLE）
SqliteHelper::Exec("INSERT INTO history (song) VALUES ('test')");

// 查询
std::vector<SqlRow> rows;
SqliteHelper::Query("SELECT * FROM history", rows);
for (const auto& row : rows) {
    // row.cols[0], row.cols[1], ...
}

// 关闭（可选，进程退出时）
SqliteHelper::Shutdown();
```

### 3.5 建表在哪里？

**不在 SqliteHelper**，而是在业务初始化流程：

```cpp
int InitDatabase() {
    if (SqliteHelper::Init("/data/ktv.db") != 0) {
        return -1;  // DB 打不开 → 程序别跑
    }

    // 建表失败 = 启动失败
    if (SqliteHelper::Exec(
        "CREATE TABLE IF NOT EXISTS history ("
        "id INTEGER PRIMARY KEY,"
        "song_id TEXT,"
        "song_name TEXT,"
        "artist TEXT,"
        "played_at INTEGER)"
    ) != 0) {
        return -1;
    }

    return 0;
}
```

### 3.6 完整使用示例

#### 插入一条历史记录

```cpp
int AddHistory(const std::string& song, const std::string& artist, const std::string& url) {
    std::string sql =
        "INSERT INTO history(song, artist, url, count) "
        "VALUES('" + song + "','" + artist + "','" + url + "',1)";
    return SqliteHelper::Exec(sql.c_str());  // 0 成功；<0 失败
}
```

#### 更新 count

```cpp
int IncreaseCount(int id) {
    std::string sql =
        "UPDATE history SET count = count + 1 WHERE id = " + std::to_string(id);
    return SqliteHelper::Exec(sql.c_str());  // 0 成功；<0 失败
}
```

#### 查询并转成业务数据

```cpp
struct HistoryItem {
    int id;
    std::string song;
    std::string artist;
    std::string url;
    int count;
};

int GetHistory(std::vector<HistoryItem>& out_history) {
    out_history.clear();

    std::vector<SqlRow> rows;
    int ret = SqliteHelper::Query(
        "SELECT id, song, artist, url, count FROM history ORDER BY count DESC",
        rows
    );
    if (ret != 0) {
        return ret;  // 查询失败
    }

    for (const auto& row : rows) {
        HistoryItem it;
        it.id     = std::atoi(row.cols[0].c_str());
        it.song   = row.cols[1];
        it.artist = row.cols[2];
        it.url    = row.cols[3];
        it.count  = std::atoi(row.cols[4].c_str());
        out_history.push_back(it);
    }
    return 0;  // 成功
}
```

#### UI 层只看到这个

```cpp
std::vector<HistoryItem> history;
if (GetHistory(history) == 0) {
    for (const auto& it : history) {
        ShowSong(it.song, it.artist, it.count);
    }
}
// UI 完全不知道：SQLite / SQL / stmt / callback / 表结构
```

### 3.7 避坑指南（必读）

#### ❌ 坑 1：在 UI 回调里调用 SqliteHelper

```cpp
// ❌ 错误：UI 回调里查询数据库
void on_btn_click(lv_event_t* e) {
    std::vector<SqlRow> rows;
    SqliteHelper::Query("SELECT * FROM history", rows);  // 阻塞 UI！
}

// ✅ 正确：UI 只触发事件，Service 层查询并返回状态
void on_btn_click(lv_event_t* e) {
    EventBus::post(QueryHistoryEvent{});
}

// Service 层处理
int HandleQueryHistory(std::vector<HistoryItem>& out) {
    return GetHistory(out);  // 返回 0 成功，<0 失败
}
```

#### ❌ 坑 2：忘记 CREATE TABLE IF NOT EXISTS

```cpp
// ❌ 错误：每次启动都重建表（数据丢失）
SqliteHelper::Exec("CREATE TABLE history (...)");

// ✅ 正确：只在不存在时创建
SqliteHelper::Exec("CREATE TABLE IF NOT EXISTS history (...)");
```

#### ❌ 坑 3：SQL 注入（用户输入拼接）

```cpp
// ❌ 危险：用户输入直接拼 SQL
std::string sql = "INSERT INTO history(song) VALUES('" + user_input + "')";
// 如果 user_input = "'); DROP TABLE history; --" 就完蛋了

// ✅ 安全做法 1：转义单引号
std::string safe_input = user_input;
// 替换 ' 为 ''
size_t pos = 0;
while ((pos = safe_input.find("'", pos)) != std::string::npos) {
    safe_input.replace(pos, 1, "''");
    pos += 2;
}

// ✅ 安全做法 2：如果是数字，用 std::to_string
std::string sql = "WHERE id = " + std::to_string(id);  // id 是 int，安全
```

#### ❌ 坑 4：Query 返回的是字符串，不是类型

```cpp
// ❌ 错误：直接当 int 用
int count = row.cols[4];  // 编译错误

// ✅ 正确：转换类型
int count = std::atoi(row.cols[4].c_str());
```

#### ❌ 坑 5：Init 失败后继续使用

```cpp
// ❌ 错误：Init 失败了还继续
SqliteHelper::Init("/readonly/path.db");  // 返回 -1
SqliteHelper::Exec("INSERT ...");  // 崩溃或静默失败

// ✅ 正确：Init 失败直接退出
if (SqliteHelper::Init("/data/ktv.db") != 0) {
    LOG_ERROR("DB init failed");
    return -1;  // 程序别跑
}
```

#### ❌ 坑 6：SELECT 列顺序和 cols 索引不对应

```cpp
// SQL: SELECT id, song, artist FROM history
// ❌ 错误：搞错顺序
std::string song = row.cols[0];  // 其实是 id

// ✅ 正确：按 SELECT 顺序
int id = std::atoi(row.cols[0].c_str());  // id
std::string song = row.cols[1];           // song
std::string artist = row.cols[2];         // artist
```

### 3.8 最佳实践清单

| 规则 | 说明 |
|------|------|
| 返回值表示状态 | 所有函数返回 `int`：0 成功，<0 失败 |
| 检查返回值 | 调用后必须检查返回值 |
| Init 一次 | 项目启动时调用一次，不要重复调用 |
| 建表用 IF NOT EXISTS | 防止数据丢失 |
| 失败即退出 | Init/建表失败 = 程序别跑 |
| Service 层专用 | UI 层禁止调用 SqliteHelper |
| 结果转类型 | Query 返回字符串，业务层用 atoi 等转换 |
| 转义单引号 | 用户输入的字符串必须转义 `'` → `''` |

### 3.9 一句话总结

```cpp
// SqliteHelper 使用原则：
// 1. 项目启动时 Init
// 2. Exec 用于写
// 3. Query 用于读
// 4. Query 返回的都是字符串，由业务层解释
```

---

## 四、JsonHelper（JSON 安全解析工具）

### 4.1 一句话定位

> **把"又臭又容易出错的 cJSON 解析细节"，变成"工程师可直接使用的安全取值函数"。**

### 4.2 使用边界（硬规则）

| 层级 | 是否允许 |
|------|---------|
| Network 层 | ✅ 允许 |
| Service 层 | ✅ 允许 |
| UI 层 | ❌ 禁止 |
| Player 层 | ❌ 禁止 |
| LVGL callback | ❌ 禁止 |

### 4.3 API 白名单（只能用这些）

```cpp
// 解析
Parse(str, len, &doc)

// 对象取值
GetString(obj, key, buf, len)
GetInt(obj, key, &out)
GetLong(obj, key, &out)
GetDouble(obj, key, &out)
GetBool(obj, key, &out)

// 数组大小
GetArraySize(arr, &out)
GetObjectArraySize(root, array_key, &out)

// 嵌套数组取值：{"items": [{...}, {...}]}
GetArrayObjectString(root, array_key, index, field_key, buf, len)
GetArrayObjectInt(root, array_key, index, field_key, &out)
GetArrayObjectBool(root, array_key, index, field_key, &out)

// 顶层数组取值：[{...}, {...}]
GetRootArrayObjectString(root_array, index, field_key, buf, len)
GetRootArrayObjectInt(root_array, index, field_key, &out)
GetRootArrayObjectBool(root_array, index, field_key, &out)
```

### 4.4 返回值规范

| 返回值 | 含义 |
|--------|------|
| `0` | 成功 |
| `-1` | 参数无效 |
| `-2` | JSON 超过大小限制 |
| `-3` | 字段不存在 |
| `-4` | 类型不匹配 |
| `-5` | 缓冲区不足（字符串被截断） |
| `-6` | JSON 解析失败 |

### 4.5 标准用法模板

#### 场景 1：解析对象

```cpp
#include "utils/json_helper.h"

void parseUser(const char* json_str, size_t len) {
    ktv::utils::JsonDocument doc;
    if (JsonHelper::Parse(json_str, len, &doc) != 0) {
        // 解析失败
        return;
    }

    char name[64];
    ktv::utils::OutInt age;

    JsonHelper::GetString(doc.root(), "name", name, sizeof(name));
    JsonHelper::GetInt(doc.root(), "age", &age);

    printf("name=%s, age=%d\n", name, age.value);
    // doc 析构时自动释放 cJSON
}
```

#### 场景 2：解析嵌套数组 `{"items": [...]}`

```cpp
void parseSongList(const char* json_str, size_t len) {
    ktv::utils::JsonDocument doc;
    if (JsonHelper::Parse(json_str, len, &doc) != 0) return;

    ktv::utils::OutInt count;
    if (JsonHelper::GetObjectArraySize(doc.root(), "items", &count) != 0) return;

    for (int i = 0; i < count.value; ++i) {
        char song_id[64], song_name[128];
        JsonHelper::GetArrayObjectString(doc.root(), "items", i, "song_id", song_id, sizeof(song_id));
        JsonHelper::GetArrayObjectString(doc.root(), "items", i, "song_name", song_name, sizeof(song_name));
        // 处理每首歌...
    }
}
```

#### 场景 3：解析顶层数组 `[{...}, {...}]`

```cpp
void parseSongArray(const char* json_str, size_t len) {
    ktv::utils::JsonDocument doc;
    if (JsonHelper::Parse(json_str, len, &doc) != 0) return;

    ktv::utils::OutInt count;
    if (JsonHelper::GetArraySize(doc.root(), &count) != 0) return;

    for (int i = 0; i < count.value; ++i) {
        char song_id[64];
        JsonHelper::GetRootArrayObjectString(doc.root(), i, "song_id", song_id, sizeof(song_id));
        // 处理每首歌...
    }
}
```

---

## 五、JsonDocument（JSON 生命周期容器）

### 5.1 设计目的

- RAII 管理 `cJSON*` 生命周期
- 析构时自动调用 `cJSON_Delete()`
- **调用方不需要手动释放**

### 5.2 特性

| 特性 | 说明 |
|------|------|
| 拷贝 | ❌ 禁止 |
| 移动 | ✅ 允许 |
| 手动释放 | 不需要（析构自动） |

### 5.3 注意事项

- `doc.root()` 返回 `const cJSON*`（只读）
- 只能传给 `JsonHelper::GetXxx()` 使用
- **禁止调用任何 cJSON 原生 API**

---

## 六、禁止事项（Code Review 一票否决）

| 禁止行为 | 原因 |
|---------|------|
| 业务代码出现 `cJSON_*` | 绕开安全边界 |
| 业务代码出现 `int* out` | 语义模糊 |
| UI 层调用 JsonHelper | 违反分层 |
| 手动调用 `cJSON_Delete()` | JsonDocument 自动管理 |
| 把 `cJSON*` 存为成员变量 | 生命周期失控 |

---

## 七、相关文档

- [C++接口与命名规范（定版）](./C++接口与命名规范（定版）.md)
- [JSON解析编码规范](./JSON解析编码规范.md)
- [代码审查Checklist](../代码审查Checklist.md)

---

**最后更新**: 2025-01-08  
**状态**: ✅ 定版

