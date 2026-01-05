# Singleton 清单（Android 风格最终版）

> **文档版本**：v1.0  
> **最后更新**：2025-12-30  
> **状态**：✅ 定版（可执行、可贴 README）  
> **适用对象**：Java/Android 背景工程师  
> **用途**：明确项目中所有合理存在的 Singleton，防止架构失控

---

## 📋 一、总览（数量感）

> **MVP 阶段允许的 Singleton 总数：8～9 个（不再多）**

| 类型 | 数量 | 说明 |
|------|------|------|
| **系统资源型 Singleton（必须）** | 4 个 | 对应 Android 系统级资源/Service |
| **架构角色型 Singleton（逻辑唯一）** | 3 个 | 对应 Android 应用结构角色 |
| **ViewModelData（领域单例）** | 2～3 个 | 按业务领域，对应 Android ViewModel.data |

**超过这个数，架构就开始"Android 化失控"。**

---

## 🔧 二、系统资源型 Singleton（Android 工程师最容易接受）

> 对应 Android 里"系统级资源 / Service"

---

### 1️⃣ NetworkClient（libcurl）

**推荐命名**：
```cpp
NetworkClient
```

**Android 心智映射**：
```
OkHttpClient / Retrofit Client
```

**为什么必须是 Singleton**：
- 连接池 / DNS / SSL 状态全局唯一
- 多实例 = 资源浪费 + 难查 bug

**职责**：
- 发 HTTP 请求
- 不关心业务
- 不持 UI

**代码示例**：
```cpp
class NetworkClient {
public:
    static NetworkClient& instance();
    
    void get(const std::string& url, std::function<void(Response)> callback);
    void post(const std::string& url, const std::string& body, std::function<void(Response)> callback);
    
private:
    NetworkClient();
    CURL* curl_handle_;
};
```

✅ **Singleton 合法**

---

### 2️⃣ MediaPlayerManager（播放器 SDK）

**推荐命名**：
```cpp
MediaPlayerManager
```

**Android 心智映射**：
```
ExoPlayer / MediaPlayer（单实例）
```

**为什么必须是 Singleton**：
- 音频输出是系统级资源
- 同一时间只能有一个播放上下文

**职责**：
- play / pause / stop
- 不决定"播什么页面"

**代码示例**：
```cpp
class MediaPlayerManager {
public:
    static MediaPlayerManager& instance();
    
    void play(const std::string& url);
    void pause();
    void stop();
    
private:
    MediaPlayerManager();
    // tplayer handle
};
```

✅ **Singleton 合法**

---

### 3️⃣ DatabaseProvider（SQLite）

**推荐命名**：
```cpp
DatabaseProvider
```

**Android 心智映射**：
```
RoomDatabase.getInstance()
```

**为什么必须是 Singleton**：
- DB 连接不能随便开
- 嵌入式环境更怕锁冲突

**职责**：
- 提供 DB handle
- 不包含业务 SQL

**代码示例**：
```cpp
class DatabaseProvider {
public:
    static DatabaseProvider& instance();
    
    sqlite3* getDatabase();
    
private:
    DatabaseProvider();
    sqlite3* db_;
};
```

✅ **Singleton 合法**

---

### 4️⃣ LogService（syslog）

**推荐命名**：
```cpp
LogService
```

**Android 心智映射**：
```
Logcat / Timber（全局）
```

**说明**：
- syslog 本身就是进程级
- 你只是包一层

**代码示例**：
```cpp
class LogService {
public:
    static LogService& instance();
    
    void error(const std::string& msg);
    void warn(const std::string& msg);
    void info(const std::string& msg);
    void debug(const std::string& msg);
    
private:
    LogService() = default;
};
```

✅ **隐式 Singleton（不额外造实例）**

---

## 🏗️ 三、架构角色型 Singleton（逻辑唯一，不是资源）

> 对应 Android 里"应用结构角色"

---

### 5️⃣ AppEventDispatcher

**推荐命名**：
```cpp
AppEventDispatcher
```

**Android 心智映射**：
```
MainThread Handler / EventRouter
```

**为什么是 Singleton**：
- 全局只有一条 Event 流
- 多 Dispatcher 会导致流程分裂

**职责**：
- 分发 Event
- 不做业务判断

**代码示例**：
```cpp
class AppEventDispatcher {
public:
    static AppEventDispatcher& instance();
    
    void dispatch(const AppEvent& event);
    void start();  // 启动 EventDispatcher 线程
    void stop();
    
private:
    AppEventDispatcher();
    std::thread dispatcher_thread_;
};
```

✅ **逻辑 Singleton 合法**

---

### 6️⃣ AppController

**推荐命名**：
```cpp
AppController
```

**Android 心智映射**：
```
ViewModel（流程控制部分）
```

**为什么是 Singleton**：
- 整个 App 只有一个"流程大脑"
- 多个 Controller = 架构灾难

**职责**：
- 接 Event
- 调 Service
- 更新 UI State

**代码示例**：
```cpp
class AppController {
public:
    static AppController& instance();
    
    // UI 入口
    void onUiCategoryClicked(int categoryId);
    
    // Service 回调入口
    void onSvcCategoryDataReady(int categoryId, void* data);
    
private:
    AppController() = default;
};
```

✅ **逻辑 Singleton 合法**

---

### 7️⃣ UiStateHolder

**推荐命名**：
```cpp
UiStateHolder
```

**Android 心智映射**：
```
UI State / LiveData State
```

**为什么是 Singleton**：
- loading / error / empty 是全局一致的
- 不应该每个页面一份

**职责**：
- 保存 UI 状态
- 不做业务判断

**代码示例**：
```cpp
class UiStateHolder {
public:
    static UiStateHolder& instance();
    
    void setLoading(bool loading);
    bool isLoading() const;
    
    void setError(const std::string& error);
    std::string getError() const;
    
private:
    UiStateHolder() = default;
    bool loading_ = false;
    std::string error_;
};
```

✅ **逻辑 Singleton 合法**

---

## 📦 四、ViewModelData（领域级 Singleton，Android 工程师最熟）

> 对应 Android 里：**每个 ViewModel 持有一份数据**

---

### 8️⃣ CategoryViewModelData

**推荐命名**：
```cpp
CategoryViewModelData
```

**Android 心智映射**：
```
CategoryViewModel.data
```

**为什么是 Singleton**：
- 同一时刻只有一份分类数据
- UI 读，Service 写

**职责**：
- 存分类列表
- 不发事件、不跑流程

**代码示例**：
```cpp
class CategoryViewModelData {
public:
    static CategoryViewModelData& instance();
    
    const std::vector<Category>& getCategories() const;
    void setCategories(const std::vector<Category>& categories);
    
private:
    CategoryViewModelData() = default;
    std::vector<Category> categories_;
};
```

✅ **领域 Singleton 合法**

---

### 9️⃣ SongViewModelData / RankViewModelData（同上）

**规则**：
- 一个业务域 = 一个 ViewModelData
- 不允许"万能 Data"

**代码示例**：
```cpp
class SongViewModelData {
public:
    static SongViewModelData& instance();
    
    const std::vector<Song>& getSongs() const;
    void setSongs(const std::vector<Song>& songs);
    
private:
    SongViewModelData() = default;
    std::vector<Song> songs_;
};
```

✅ **领域 Singleton 合法**

---

## 🚫 五、明确禁止出现的 Singleton（Android 工程师最容易踩的坑）

> 这些名字一出现，你可以**立刻叫停**

---

### ❌ UIManager

**Android 误区**：
```
一个对象管所有页面
```

**为什么禁止**：
- 会吞掉 Controller
- 会绕过 Event
- 会变成"上帝对象"

**正确做法**：
- 使用 `PageManager`（非 Singleton，管理页面生命周期）
- 使用 `AppController`（Singleton，处理业务逻辑）

🚫 **禁止 Singleton**

---

### ❌ ServiceManager

**Android 误区**：
```
getService("category")
```

**为什么禁止**：
- Controller 已经是调度中心
- 不需要额外的 Service 管理器

**正确做法**：
- 直接调用 `CategoryService::instance()`
- 通过 `AppController` 统一调度

🚫 **禁止 Singleton**

---

### ❌ ViewModelManager

**为什么禁止**：
- ViewModelData 已经足够
- 不需要额外的管理器

**正确做法**：
- 直接使用 `CategoryViewModelData::instance()`

🚫 **禁止 Singleton**

---

### ❌ ThreadManager / TaskManager

**为什么禁止**：
- MVP 阶段不允许线程池
- 线程应该由 Service 内部管理

**正确做法**：
- NetworkClient 内部管理网络线程
- MediaPlayerManager 内部管理播放线程

🚫 **禁止 Singleton**

---

## 📊 六、最终汇总表（可以直接贴）

| 类型 | 名称（Android 风格） | Android 等价物 | 是否允许 Singleton | 说明 |
|------|---------------------|---------------|-------------------|------|
| **网络** | NetworkClient | OkHttpClient / Retrofit | ✅ | libcurl 封装 |
| **播放** | MediaPlayerManager | ExoPlayer / MediaPlayer | ✅ | tplayer 封装 |
| **数据库** | DatabaseProvider | RoomDatabase | ✅ | SQLite 封装 |
| **日志** | LogService | Logcat / Timber | ✅ | syslog 封装 |
| **事件** | AppEventDispatcher | MainThread Handler | ✅ | Event 分发 |
| **流程** | AppController | ViewModel（流程部分） | ✅ | 业务编排 |
| **UI 状态** | UiStateHolder | LiveData State | ✅ | UI 状态管理 |
| **业务数据** | XXXViewModelData | ViewModel.data | ✅ | 领域数据 |
| **UI 管理** | UIManager | ❌ | ❌ | 禁止 |
| **Service 管理** | ServiceManager | ❌ | ❌ | 禁止 |
| **ViewModel 管理** | ViewModelManager | ❌ | ❌ | 禁止 |
| **线程管理** | ThreadManager | ❌ | ❌ | 禁止 |

---

## 🎯 七、终极判断标准（你可以直接对团队说）

你以后只需要问一句：

> **这个 Singleton，  
> 在 Android 里有没有等价物？**

- ✅ **有**（OkHttp / ViewModel / Room）→ 可能合理
- ❌ **没有**（UIManager / ServiceManager）→ 直接拒绝

---

## 📝 八、Singleton 实现模板（标准版）

### 标准模板

```cpp
class SingletonName {
public:
    static SingletonName& instance() {
        static SingletonName inst;
        return inst;
    }
    
    // 删除拷贝和移动
    SingletonName(const SingletonName&) = delete;
    SingletonName& operator=(const SingletonName&) = delete;
    SingletonName(SingletonName&&) = delete;
    SingletonName& operator=(SingletonName&&) = delete;
    
    // 公共接口
    void publicMethod();
    
private:
    // 私有构造函数
    SingletonName();
    ~SingletonName() = default;
    
    // 成员变量
    int member_;
};
```

### 注意事项

1. **禁止在构造函数中调用其他 Singleton**：
   ```cpp
   // ❌ 错误
   SingletonName() {
       OtherSingleton::instance();  // 禁止！
   }
   ```

2. **禁止在析构函数中阻塞**：
   ```cpp
   // ❌ 错误
   ~SingletonName() {
       thread_.join();  // 禁止！应该在 stop() 中显式调用
   }
   ```

3. **必须提供显式的 stop/cleanup 方法**：
   ```cpp
   // ✅ 正确
   void stop() {
       running_ = false;
       thread_.join();
   }
   ```

---

## 📚 九、相关文档

- [模块责任表（RACI最终版）.md](./模块责任表（RACI最终版）.md) ⭐⭐⭐ **必读**
- [Tina_KTV项目10条铁律（贴墙版）.md](./Tina_KTV项目10条铁律（贴墙版）.md) ⭐⭐⭐ **必读**
- [代码目录结构等于模块责任结构（定版）.md](./代码目录结构等于模块责任结构（定版）.md) ⭐⭐⭐ **必读**
- [KTV_App线程Singleton编码规范（最终版）.md](./KTV_App线程Singleton编码规范（最终版）.md) ⭐⭐⭐ **必读**

---

## 💡 十、最后一段实话

你现在这套 Singleton 列表：

- ✅ **数量是受控的**（8～9 个）
- ✅ **名字是"工程师友好"的**（Android 风格）
- ✅ **行为边界是清晰的**（职责明确）
- ✅ **判断标准是简单的**（Android 等价物）

这已经是**非常成熟的 Android → 嵌入式迁移架构**了。

---

**最后更新**: 2025-12-30  
**状态**: ✅ 定版（可执行、可贴 README）  
**维护者**: Tech Product Owner

