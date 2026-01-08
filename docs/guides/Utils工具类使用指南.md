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
└── log_macros.h       ← 日志语法糖（统一格式）
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

## 三、JsonHelper（JSON 安全解析工具）

### 2.1 一句话定位

> **把"又臭又容易出错的 cJSON 解析细节"，变成"工程师可直接使用的安全取值函数"。**

### 2.2 使用边界（硬规则）

| 层级 | 是否允许 |
|------|---------|
| Network 层 | ✅ 允许 |
| Service 层 | ✅ 允许 |
| UI 层 | ❌ 禁止 |
| Player 层 | ❌ 禁止 |
| LVGL callback | ❌ 禁止 |

### 2.3 API 白名单（只能用这些）

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

### 2.4 返回值规范

| 返回值 | 含义 |
|--------|------|
| `0` | 成功 |
| `-1` | 参数无效 |
| `-2` | JSON 超过大小限制 |
| `-3` | 字段不存在 |
| `-4` | 类型不匹配 |
| `-5` | 缓冲区不足（字符串被截断） |
| `-6` | JSON 解析失败 |

### 2.5 标准用法模板

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

## 四、JsonDocument（JSON 生命周期容器）

### 3.1 设计目的

- RAII 管理 `cJSON*` 生命周期
- 析构时自动调用 `cJSON_Delete()`
- **调用方不需要手动释放**

### 3.2 特性

| 特性 | 说明 |
|------|------|
| 拷贝 | ❌ 禁止 |
| 移动 | ✅ 允许 |
| 手动释放 | 不需要（析构自动） |

### 3.3 注意事项

- `doc.root()` 返回 `const cJSON*`（只读）
- 只能传给 `JsonHelper::GetXxx()` 使用
- **禁止调用任何 cJSON 原生 API**

---

## 五、禁止事项（Code Review 一票否决）

| 禁止行为 | 原因 |
|---------|------|
| 业务代码出现 `cJSON_*` | 绕开安全边界 |
| 业务代码出现 `int* out` | 语义模糊 |
| UI 层调用 JsonHelper | 违反分层 |
| 手动调用 `cJSON_Delete()` | JsonDocument 自动管理 |
| 把 `cJSON*` 存为成员变量 | 生命周期失控 |

---

## 六、相关文档

- [C++接口与命名规范（定版）](./C++接口与命名规范（定版）.md)
- [JSON解析编码规范](./JSON解析编码规范.md)
- [代码审查Checklist](../代码审查Checklist.md)

---

**最后更新**: 2025-01-08  
**状态**: ✅ 定版

