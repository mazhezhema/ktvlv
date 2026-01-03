# JSON解析编码规范（cJSON使用规范）

> **文档版本**：v1.0  
> **最后更新**：2025-12-30  
> **状态**：✅ 核心文档  
> **相关文档**：详见 [C++编码规范与避坑指南.md](./C++编码规范与避坑指南.md)

---

## 🎯 核心原则

### ✅ 一句话结论（可直接定稿）

> **JSON 作为"传输格式"，可以是字符串；
> 但在系统内部，绝不应该以字符串形态传播。
> 系统内部必须传递"结构化对象"。**

这是**工程边界问题，不是 JSON 技术问题**。

### ✅ 正确定位

> **cJSON 是一个 JSON → C struct 的转换工具**
> **解析完就丢，不在系统里长期持有 cJSON 对象**

### ❌ 错误定位

> ~~"cJSON 是我们的 JSON 数据模型"~~
> ~~"系统内部可以传递 JSON 字符串"~~

---

## 📋 标准处理流程（必须遵守）

### ✅ 推荐的标准流水线

```
HTTP 返回 char* (网络层)
        ↓
cJSON_ParseWithLength（带长度限制，在 Worker 线程）
        ↓
只读取需要的字段（白名单解析）
        ↓
拷贝到预分配 struct
        ↓
cJSON_Delete（立即释放）
        ↓
后续模块只操作 struct（不接触 JSON）
```

**关键点**：

* ✅ cJSON 生命周期 < 1 个函数
* ✅ 不跨模块
* ✅ 不存成员变量
* ✅ 不缓存树
* ✅ 解析失败 = 本次请求失败（不做部分成功）
* ✅ **JSON 只存在于网络层，模块间只传 struct**

### ✅ JSON 生命周期模型（强烈建议）

```
HTTP recv (char*)          ← JSON 字符串只在这里存在
   ↓
解析（cJSON / JsonHelper）   ← 在 Worker 线程解析
   ↓
映射到 struct              ← 立即转换为结构化对象
   ↓
释放 JSON                  ← JSON 生命周期结束
   ↓
后续模块只接触 struct       ← 系统内部只传递对象
```

> **JSON 的生命周期 ≤ 一个函数**
> **模块间传递的契约 = 结构化对象，不是字符串**

---

## 🚫 禁止项（必须遵守）

### ❌ 禁止 1：把 cJSON 对象存为成员变量

```cpp
// ❌ 错误：cJSON 对象作为成员变量
class SongService {
    cJSON* json_cache_;  // 禁止！
public:
    void parse(const char* json_str) {
        json_cache_ = cJSON_Parse(json_str);  // 禁止！
    }
};
```

### ❌ 禁止 2：跨函数传递 cJSON 对象

```cpp
// ❌ 错误：跨函数传递 cJSON
cJSON* parseJson(const char* str) {
    return cJSON_Parse(str);  // 禁止返回 cJSON*
}

void useJson() {
    cJSON* json = parseJson(str);
    // ... 使用
    // 容易忘记 cJSON_Delete(json)！
}
```

### ❌ 禁止 3：在 UI 线程解析 JSON

```cpp
// ❌ 错误：在 UI 线程解析
void onButtonClick() {
    cJSON* json = cJSON_Parse(response);  // 禁止！
    // 会阻塞 LVGL 渲染
}
```

### ❌ 禁止 4：把 JSON 原文传给 UI

```cpp
// ❌ 错误：JSON 字符串传给 UI
void updateUI(const char* json_str) {
    ui->setData(json_str);  // 禁止！
}
```

### ❌ 禁止 5：在模块间传递 JSON 字符串

```cpp
// ❌ 错误：模块间传递 JSON 字符串
void NetworkThread::onHttpResponse(const char* json_str) {
    // 禁止直接传递 JSON 字符串
    UiEventQueue::push(JsonStringEvent{json_str});  // 禁止！
}

// ❌ 错误：Service 层接收 JSON 字符串
void SongService::parse(const char* json_str) {
    // 禁止在 Service 层接收 JSON 字符串
}
```

**为什么禁止？**

* ❌ 重复解析：同一份 JSON 被 parse 多次
* ❌ 解析位置失控：有人为了方便在 UI 线程 parse
* ❌ 模块强耦合 API 格式：API 改字段名，全崩
* ❌ 生命周期和内存灾难：谁 malloc？谁 free？
* ❌ 字符串不是"契约"：字段是否存在？类型是什么？全是隐式约定

### ❌ 禁止 6：不做大小限制

```cpp
// ❌ 错误：无大小限制
cJSON* json = cJSON_Parse(huge_json_string);  // 危险！
```

### ❌ 禁止 7：部分成功继续执行

```cpp
// ❌ 错误：部分字段缺失继续执行
if (!id) {
    // 跳过这个字段，继续解析其他字段  // 禁止！
}
```

---

## ✅ 正确做法（模板）

### ✅ 模板 1：标准解析流程

```cpp
#include "json_helper.h"  // 使用封装的 JsonHelper

bool SongService::parseSongList(const char* json_str, size_t len, 
                                 SongList* out) {
    // 1. 大小限制检查（必须）
    if (len > MAX_JSON_SIZE) {
        LOG_ERROR("JSON too large: %zu bytes", len);
        return false;
    }
    
    // 2. 解析（使用 JsonHelper 封装）
    cJSON* root = cJSON_ParseWithLength(json_str, len);
    if (!root) {
        LOG_ERROR("JSON parse failed");
        return false;
    }
    
    // 3. 只读取需要的字段（白名单）
    cJSON* items = cJSON_GetObjectItem(root, "items");
    if (!cJSON_IsArray(items)) {
        cJSON_Delete(root);
        return false;
    }
    
    int count = cJSON_GetArraySize(items);
    for (int i = 0; i < count && i < MAX_SONGS; ++i) {
        cJSON* item = cJSON_GetArrayItem(items, i);
        Song& song = out->songs[i];
        
        // 使用 JsonHelper 安全读取
        if (!JsonHelper::getString(item, "song_id", song.id, sizeof(song.id))) {
            continue;  // 关键字段缺失，跳过这条
        }
        JsonHelper::getString(item, "song_name", song.title, sizeof(song.title));
        JsonHelper::getString(item, "artist", song.artist, sizeof(song.artist));
        JsonHelper::getString(item, "m3u8_url", song.url, sizeof(song.url));
        
        out->count++;
    }
    
    // 4. 立即释放（必须）
    cJSON_Delete(root);
    root = nullptr;
    
    return true;
}
```

### ✅ 模板 2：在 Worker 线程解析

```cpp
// 在业务线程（Worker Thread）中解析
void NetworkThread::onHttpResponse(const char* json_str, size_t len) {
    SongList list;
    
    // 解析 JSON（在 Worker 线程）
    if (!songService_.parseSongList(json_str, len, &list)) {
        // 解析失败，发送错误事件
        UiEventQueue::push(ErrorEvent{"JSON parse failed"});
        return;
    }
    
    // 发送成功事件到 UI 线程（只传 struct，不传 JSON）
    UiEventQueue::push(SongListEvent{list});
}
```

### ✅ 模板 3：UI 线程只接收 struct

```cpp
// UI 线程接收事件
void HomePage::onEvent(const SongListEvent& event) {
    // 直接使用 struct，不涉及 JSON
    for (int i = 0; i < event.list.count; ++i) {
        addSongItem(event.list.songs[i]);
    }
}
```

---

## 🚨 系统边界原则（核心架构原则）

### ✅ 原则 1：JSON 只存在于网络层

```cpp
// ✅ 正确：JSON 只在网络层存在
void NetworkThread::onHttpResponse(const char* json_str, size_t len) {
    SongList list;
    
    // 在网络层解析 JSON
    if (!parseJson(json_str, len, &list)) {
        return;
    }
    
    // 立即转换为 struct，不再传递 JSON
    UiEventQueue::push(SongListEvent{list});  // ✅ 只传 struct
}
```

### ✅ 原则 2：模块间只传 struct

```cpp
// ✅ 正确：Service 层接口只接收 struct
class SongService {
public:
    // ✅ 正确：接收 struct
    void updateSongList(const SongList& list);
    
    // ❌ 错误：接收 JSON 字符串
    // void updateSongList(const char* json_str);  // 禁止！
};
```

### ✅ 原则 3：任何模块不直接 parse JSON

```cpp
// ❌ 错误：UI 层 parse JSON
void HomePage::onEvent(const char* json_str) {
    cJSON* json = cJSON_Parse(json_str);  // 禁止！
}

// ✅ 正确：UI 层只接收 struct
void HomePage::onEvent(const SongListEvent& event) {
    // 直接使用 struct，不涉及 JSON
    for (int i = 0; i < event.list.count; ++i) {
        addSongItem(event.list.songs[i]);
    }
}
```

### ✅ 原则 4：JSON 生命周期不跨模块

| 场景 | JSON 字符串是否合理 |
|------|------------------|
| 网络接收层 | ✅ 合理 |
| 日志打印 | ✅ 合理 |
| 调试 / dump | ✅ 合理 |
| 原样缓存（短生命周期） | ⚠️ 谨慎使用 |
| **模块边界传递** | ❌ **禁止** |

---

## 🛡️ 防御策略（必须实现）

### 1️⃣ 大小限制（必须）

```cpp
// 在 json_helper.h 中定义
#define MAX_JSON_SIZE (64 * 1024)  // 64KB 上限

bool JsonHelper::parse(const char* str, size_t len, cJSON** out) {
    if (len > MAX_JSON_SIZE) {
        LOG_ERROR("JSON size exceeds limit: %zu > %d", len, MAX_JSON_SIZE);
        return false;
    }
    // ...
}
```

### 2️⃣ 线程隔离（必须）

| 操作 | 允许的线程 | 禁止的线程 |
|------|-----------|-----------|
| JSON 解析 | ✅ Worker 线程 | ❌ UI 线程 |
| JSON 解析 | ✅ 业务线程 | ❌ LVGL tick |
| JSON 解析 | ✅ 网络线程 | ❌ 播放回调 |

### 3️⃣ 失败即丢（必须）

```cpp
// ✅ 正确：解析失败立即返回
if (!parseJson(str, len, &list)) {
    return false;  // 不做部分成功
}

// ❌ 错误：部分字段缺失继续执行
if (!id) {
    song.id = "";  // 禁止！
    // 继续解析其他字段
}
```

### 4️⃣ 接口分类处理（推荐）

| 接口类型 | 客户端策略 |
|---------|-----------|
| **状态/控制** | cJSON 全解析 |
| **列表/搜索** | cJSON 只做结构定位，只取前 N 条 |
| **首页** | 只取前 N 条 |
| **翻页** | 分次请求 |

---

## 📦 JsonHelper 封装（必须使用）

### 🎯 一句话结论（先拍板）

> **JsonHelper 不是过度设计，它是"把复杂度锁在一个点上"的必要设计。**
> **但 JsonHelper 只能很薄，一旦变厚，就是过度设计。**

### 设计目的（不是"封装库"）

> **JsonHelper 的目的只有一个：防止"cJSON 使用方式在项目中失控"。**

它不是：
* 新的一套 JSON 框架 ❌
* 数据模型层 ❌
* 网络层 ❌

它是：
* **使用规范的执行器** ✔

### 设计原则

> **业务层禁止直接使用 cJSON API**
> **必须通过 JsonHelper 封装**

### ✅ 正确的 JsonHelper 应该是这样的

* 一个 `.h + .cpp`
* 10～15 个函数
* 无状态
* 不持有对象
* 不隐藏 cJSON

### ❌ 过度设计的 JsonHelper 是这样的

* 封装成一个类体系
* 管生命周期
* 存 cJSON*
* 提供 `JsonObject`, `JsonArray` 抽象

👉 **这是造新轮子，禁止！**

### JsonHelper 接口设计（薄封装）

```cpp
// json_helper.h
#pragma once
#include "cJSON.h"
#include <stdbool.h>
#include <stddef.h>

#define MAX_JSON_SIZE (64 * 1024)  // 64KB 上限

/**
 * JsonHelper - cJSON 封装工具类（薄封装）
 * 
 * 设计目的：防止"cJSON 使用方式在项目中失控"
 * 设计原则：很薄（10-15个函数）、无状态、不持有对象、不隐藏cJSON
 * 使用边界：只被 Network/Service 层调用，禁止 UI/Player 层调用
 */
class JsonHelper {
public:
    // 安全解析（带大小检查）
    static cJSON* parse(const char* str, size_t len);
    
    // 安全读取字符串（带缓冲区保护）
    static bool getString(cJSON* obj, const char* key, 
                          char* out, size_t out_len);
    
    // 安全读取整数
    static bool getInt(cJSON* obj, const char* key, int* out);
    
    // 安全读取浮点数
    static bool getDouble(cJSON* obj, const char* key, double* out);
    
    // 安全读取布尔值
    static bool getBool(cJSON* obj, const char* key, bool* out);
    
    // 获取数组大小（带检查）
    static int getArraySize(cJSON* arr);
    
    // 获取数组项（带检查）
    static cJSON* getArrayItem(cJSON* arr, int index);
    
    // 获取对象项（带检查）
    static cJSON* getObjectItem(cJSON* obj, const char* key);
};
```

**特点**：
* ✅ 很薄：10-15个函数
* ✅ 无状态：所有函数都是 static
* ✅ 不持有对象：不存储 cJSON* 指针
* ✅ 不隐藏 cJSON：仍然返回 `cJSON*`，调用方负责释放

### JsonHelper 在系统里的正确位置

```
HTTPClient (网络层)
   ↓
JsonHelper   ← 只在这里（Network/Service层）
   ↓
Service / Model (struct)
   ↓
UI / Player (不接触JSON)
```

### 谁来调用 JsonHelper？（这是重点）

> **只有一个调用者：Network / Service 层**

| 层 | 是否允许调用 JsonHelper |
|------|----------------------|
| **Network 层** | ✅ 允许（JSON解析） |
| **Service 层** | ✅ 允许（JSON解析） |
| **UI 层** | ❌ 禁止 |
| **Player 层** | ❌ 禁止 |
| **LVGL callback** | ❌ 禁止 |
| **音频线程** | ❌ 禁止 |

👉 **JsonHelper 是"网络解析工具"，不是"通用工具"**

### JsonHelper 的真实价值

> **JsonHelper 的价值不在"省代码"，而在"防止错误扩散"。**

在嵌入式 + 多线程 + 长周期项目里：
* 错误不是写错
* 是"用法失控"

JsonHelper 就是**拦截器**。

### JsonHelper 模块职责（定稿版）

> **JsonHelper 模块职责：**
>
> * 提供 cJSON 的安全访问函数
> * 统一错误处理
> * 约束 JSON 使用方式
> * 仅用于 Network / Service 层
> * 不持有 JSON 对象
> * 不参与业务逻辑

---

## 📝 使用示例（完整流程）

### 示例 1：解析歌曲列表

```cpp
// song_service.cpp
#include "json_helper.h"

bool SongService::parseSongList(const char* json_str, size_t len, 
                                 SongList* out) {
    // 1. 使用 JsonHelper 解析
    cJSON* root = JsonHelper::parse(json_str, len);
    if (!root) {
        return false;
    }
    
    // 2. 获取数组
    cJSON* items = JsonHelper::getObjectItem(root, "items");
    if (!items || !cJSON_IsArray(items)) {
        cJSON_Delete(root);
        return false;
    }
    
    // 3. 遍历数组（只取需要的字段）
    int count = JsonHelper::getArraySize(items);
    out->count = 0;
    
    for (int i = 0; i < count && i < MAX_SONGS; ++i) {
        cJSON* item = JsonHelper::getArrayItem(items, i);
        if (!item) continue;
        
        Song& song = out->songs[out->count];
        
        // 关键字段必须存在
        if (!JsonHelper::getString(item, "song_id", song.id, sizeof(song.id))) {
            continue;  // 跳过这条
        }
        
        // 可选字段
        JsonHelper::getString(item, "song_name", song.title, sizeof(song.title));
        JsonHelper::getString(item, "artist", song.artist, sizeof(song.artist));
        JsonHelper::getString(item, "m3u8_url", song.url, sizeof(song.url));
        
        out->count++;
    }
    
    // 4. 立即释放
    cJSON_Delete(root);
    
    return out->count > 0;
}
```

### 示例 2：在 Worker 线程中使用

```cpp
// network_thread.cpp
void NetworkThread::onHttpResponse(const HttpResponse& resp) {
    if (resp.status_code != 200) {
        UiEventQueue::push(ErrorEvent{"HTTP error"});
        return;
    }
    
    // 在 Worker 线程解析
    SongList list;
    if (!songService_.parseSongList(resp.body, resp.body_len, &list)) {
        UiEventQueue::push(ErrorEvent{"JSON parse failed"});
        return;
    }
    
    // 发送到 UI 线程（只传 struct）
    UiEventQueue::push(SongListEvent{list});
}
```

---

## ✅ 检查清单（代码审查用）

### 必须检查项

- [ ] ✅ cJSON 对象生命周期 < 1 个函数
- [ ] ✅ 解析后立即 `cJSON_Delete`
- [ ] ✅ 不在成员变量中存储 cJSON 对象
- [ ] ✅ 不在 UI 线程解析 JSON
- [ ] ✅ 使用 `JsonHelper` 封装，不直接调用 cJSON API
- [ ] ✅ **`JsonHelper` 只在 Network/Service 层调用**（边界检查）
- [ ] ✅ 有大小限制检查（`MAX_JSON_SIZE`）
- [ ] ✅ 解析失败立即返回，不做部分成功
- [ ] ✅ 关键字段缺失时跳过该条记录
- [ ] ✅ 不把 JSON 字符串传给 UI，只传 struct
- [ ] ✅ **不在模块间传递 JSON 字符串**（核心原则）
- [ ] ✅ **JSON 只存在于网络层，系统内部只传 struct**
- [ ] ✅ **任何模块不直接 parse JSON（除了网络层）**

---

## 🎯 总结

### 核心原则

1. ✅ **cJSON 是工具，不是模型** - 解析完就丢
2. ✅ **必须封装** - 业务层禁止直接使用 cJSON API
3. ✅ **JsonHelper 很薄** - 10-15个函数，无状态，不持有对象，不隐藏cJSON
4. ✅ **JsonHelper 边界清晰** - 只被 Network/Service 层调用，禁止 UI/Player 层调用
5. ✅ **线程隔离** - 只在 Worker 线程解析
6. ✅ **大小限制** - 必须有上限保护
7. ✅ **失败即丢** - 不做部分成功
8. ✅ **系统边界** - JSON 只存在于网络层，模块间只传 struct

### 系统边界原则（可直接定稿）

> **原则 1**：JSON 只存在于网络层  
> **原则 2**：模块间只传 struct  
> **原则 3**：任何模块不直接 parse JSON（除了网络层）  
> **原则 4**：JSON 生命周期不跨模块

### 为什么"不能在系统内部传 JSON 字符串"？

| 问题 | 字符串传递 | 对象传递 |
|------|----------|---------|
| **重复解析** | ❌ 同一份 JSON 被 parse 多次 | ✅ 只 parse 一次 |
| **解析位置失控** | ❌ 有人为了方便在 UI 线程 parse | ✅ 只在 Worker 线程 parse |
| **模块强耦合** | ❌ API 改字段名，全崩 | ✅ 接口不变，只改解析层 |
| **生命周期混乱** | ❌ 谁 malloc？谁 free？ | ✅ 固定 struct，明确生命周期 |
| **契约不明确** | ❌ 字段是否存在？类型是什么？ | ✅ 字段固定，类型固定 |

> **字符串是"方便"，对象是"专业"。**  
> 在嵌入式、多线程、要量产、要维护的项目中，**继续传 JSON 字符串，迟早失控。**

### 工程价值

| 维度 | 评价 |
|------|------|
| **稳定性** | ⭐⭐⭐⭐⭐ |
| **工程风险** | ⭐⭐⭐⭐⭐（低） |
| **维护成本** | ⭐⭐⭐⭐⭐ |
| **性能上限** | ⭐⭐⭐（靠策略补） |
| **量产友好** | ⭐⭐⭐⭐⭐ |

---

## 📚 相关文档

- **C++编码规范**: [C++编码规范与避坑指南.md](./C++编码规范与避坑指南.md)
- **开源库选型**: [开源库选型指南.md](./开源库选型指南.md)
- **代码审查**: [代码审查Checklist.md](../代码审查Checklist.md)

---

**最后更新**: 2025-12-30

