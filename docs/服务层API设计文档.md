# 服务层 API 设计文档

> **文档版本**：v1.0  
> **最后更新**：2025-12-30  
> **状态**：✅ 核心文档  
> **适用对象**：业务层开发工程师  
> **相关文档**：详见 [团队开发规范v1.md](./团队开发规范v1.md)

---

## 📌 概述

本文档定义所有服务层的 API 接口，业务层开发工程师只需调用这些接口，无需关心底层实现细节。

**核心原则**：
- ✅ 所有接口都是线程安全的
- ✅ 所有接口都是同步的（内部异步处理）
- ✅ 所有接口都不暴露锁、线程、网络细节
- ✅ **所有接口只传递结构化对象（struct），不传递 JSON 字符串**

> **系统边界原则**：JSON 只存在于网络层，模块间只传 struct。详见 [JSON解析编码规范.md](./guides/JSON解析编码规范.md)

---

## 🎧 PlayerService（播放器服务）

### 接口定义

```cpp
class PlayerService {
public:
    static PlayerService& instance();
    
    // ========== 播放控制 ==========
    
    /**
     * 播放歌曲
     * @param url 歌曲URL（m3u8地址或本地file://路径）
     */
    void play(const std::string& url);
    
    /**
     * 暂停播放
     */
    void pause();
    
    /**
     * 继续播放
     */
    void resume();
    
    /**
     * 停止播放
     */
    void stop();
    
    /**
     * 下一首
     */
    void next();
    
    /**
     * 上一首
     */
    void previous();
    
    /**
     * 重唱（seek到开头）
     */
    void replay();
    
    /**
     * 切换音轨（原唱/伴奏）
     * @param track 0=原唱, 1=伴奏
     */
    void switchTrack(int track);
    
    /**
     * 设置音量
     * @param volume 音量值 0-100
     */
    void setVolume(int volume);
    
    /**
     * 获取当前音量
     * @return 音量值 0-100
     */
    int getVolume() const;
    
    // ========== 状态查询 ==========
    
    /**
     * 获取播放状态
     * @return PlayerState {IDLE, PREPARING, PLAYING, PAUSED, STOPPED, ERROR}
     */
    PlayerState getState() const;
    
    /**
     * 获取当前播放位置（毫秒）
     * @return 播放位置
     */
    int getPosition() const;
    
    /**
     * 获取总时长（毫秒）
     * @return 总时长
     */
    int getDuration() const;
    
    /**
     * 获取当前播放的URL
     * @return URL
     */
    std::string getCurrentUrl() const;
    
    // ========== 事件监听 ==========
    
    /**
     * 监听播放状态变化
     * @param callback 回调函数
     */
    void onStateChanged(std::function<void(PlayerState)> callback);
    
    /**
     * 监听播放进度
     * @param callback 回调函数（参数：当前位置毫秒）
     */
    void onProgress(std::function<void(int ms)> callback);
    
    /**
     * 监听播放完成
     * @param callback 回调函数
     */
    void onCompleted(std::function<void()> callback);
    
    /**
     * 监听播放错误
     * @param callback 回调函数（参数：错误码, 错误信息）
     */
    void onError(std::function<void(int code, const std::string& message)> callback);
};
```

### 使用示例

```cpp
// 播放歌曲
PlayerService::instance().play("http://example.com/song.m3u8");

// 暂停
PlayerService::instance().pause();

// 切换音轨
PlayerService::instance().switchTrack(1); // 切换到伴奏

// 设置音量
PlayerService::instance().setVolume(80);

// 监听状态变化
PlayerService::instance().onStateChanged([](PlayerState state) {
    if (state == PlayerState::PLAYING) {
        Logger::info("开始播放");
    } else if (state == PlayerState::PAUSED) {
        Logger::info("已暂停");
    }
});
```

---

## 🌐 NetworkService（网络服务）

> **⚠️ 重要说明**：网络服务采用异步Event驱动架构，不使用同步返回接口。所有网络请求结果通过Event队列返回。  
> **相关文档**：[NetworkService与libcurl实现指南（MVP可落地版）.md](./guides/NetworkService与libcurl实现指南（MVP可落地版）.md) ⭐⭐⭐ **必读**

### 接口定义

```cpp
class NetworkService {
public:
    static NetworkService& instance();
    
    // ========== 初始化 ==========
    
    /**
     * 初始化服务（在Network Worker线程启动时调用）
     * @return true 成功，false 失败
     */
    bool init();
    
    /**
     * 清理服务（在Network Worker线程停止时调用）
     */
    void cleanup();
    
    // ========== HTTP GET请求（异步，结果通过Event返回）==========
    
    /**
     * 获取分类数据
     * @param categoryId 分类ID
     * 结果通过 EventType::EVENT_CATEGORY_DATA_READY 事件返回
     */
    void fetchCategory(int categoryId);
    
    /**
     * 搜索歌曲
     * @param keyword 搜索关键词
     * 结果通过 EventType::EVENT_SEARCH_RESULT_READY 事件返回
     */
    void fetchSearch(const std::string& keyword);
    
    /**
     * 获取歌曲列表
     * @param page 页码
     * @param size 每页大小
     * 结果通过 EventType::EVENT_SONG_LIST_READY 事件返回
     */
    void fetchSongList(int page, int size);
    
    // ========== HTTP POST请求（异步，结果通过Event返回）==========
    
    /**
     * 添加歌曲到播放队列
     * @param songId 歌曲ID
     * 结果通过 EventType::EVENT_QUEUE_ADD_RESULT 事件返回
     */
    void postQueueAdd(int songId);
    
    /**
     * 登录
     * @param username 用户名
     * @param password 密码
     * 结果通过 EventType::EVENT_LOGIN_RESULT 事件返回
     */
    void postLogin(const std::string& username, const std::string& password);
};
```

### 使用示例

```cpp
// ✅ 正确：异步请求，结果通过Event返回
// 在CategoryService中调用
NetworkService::instance().fetchCategory(123);

// 在EventDispatcher中处理结果事件
case EventType::EVENT_CATEGORY_DATA_READY:
    CategoryService::instance().onDataReady(ev.arg1, ev.data);
    break;

// ✅ 正确：网络请求失败也通过Event返回
case EventType::EVENT_NETWORK_ERROR:
    // 处理网络错误
    break;
```

### 核心原则

1. **异步Event驱动**：所有网络请求都是异步的，结果通过EventQueue返回
2. **libcurl全局唯一**：libcurl只在NetworkService中使用，Singleton模式
3. **回调只收数据**：libcurl回调只负责接收数据，不包含业务逻辑
4. **避免回调地狱**：网络线程 → push event → Service收结果 → UI刷新

**详细实现说明请参考**：[NetworkService与libcurl实现指南（MVP可落地版）.md](./guides/NetworkService与libcurl实现指南（MVP可落地版）.md)

---

<!-- WebSocketService（非MVP，暂不提供） -->

## 💾 CacheService（缓存服务）

### 接口定义

```cpp
class CacheService {
public:
    static CacheService& instance();
    
    // ========== 字符串缓存 ==========
    
    /**
     * 设置缓存
     * @param key 缓存键
     * @param value 缓存值
     * @param ttl 过期时间（秒），0表示永不过期
     */
    void set(const std::string& key, const std::string& value, int ttl = 0);
    
    /**
     * 获取缓存
     * @param key 缓存键
     * @return 缓存值，如果不存在或已过期返回空字符串
     */
    std::string get(const std::string& key);
    
    /**
     * 删除缓存
     * @param key 缓存键
     */
    void remove(const std::string& key);
    
    /**
     * 检查缓存是否存在
     * @param key 缓存键
     * @return true=存在, false=不存在
     */
    bool exists(const std::string& key);
    
    // ========== 文件缓存 ==========
    
    /**
     * 缓存文件（下载并保存）
     * @param url 文件URL
     * @param localPath 本地保存路径（相对于缓存目录）
     * @return 本地文件路径（完整路径），失败返回空字符串
     */
    std::string cacheFile(const std::string& url, const std::string& localPath);
    
    /**
     * 获取缓存文件路径
     * @param localPath 本地路径（相对于缓存目录）
     * @return 完整文件路径，如果不存在返回空字符串
     */
    std::string getCachedFilePath(const std::string& localPath);
    
    /**
     * 清理过期缓存
     */
    void cleanup();
    
    /**
     * 获取缓存目录
     * @return 缓存目录路径
     */
    std::string getCacheDir() const;
};
```

### 使用示例

```cpp
// 字符串缓存
CacheService::instance().set("chart_top100", jsonData, 3600); // 缓存1小时
auto cached = CacheService::instance().get("chart_top100");
if (!cached.empty()) {
    // 使用缓存数据
}

// 文件缓存
auto filePath = CacheService::instance().cacheFile(
    "http://example.com/image.jpg", 
    "images/cover.jpg"
);
if (!filePath.empty()) {
    // 使用本地文件路径
}
```

---

## 📥 DownloadService（下载服务）

### 接口定义

```cpp
struct DownloadProgress {
    int64_t downloaded;  // 已下载字节数
    int64_t total;       // 总字节数（-1表示未知）
    double speed;        // 下载速度（字节/秒）
    double percent;      // 下载进度（0-100）
};

class DownloadService {
public:
    static DownloadService& instance();
    
    // ========== 下载任务 ==========
    
    /**
     * 下载文件
     * @param url 文件URL
     * @param savePath 保存路径（完整路径）
     * @return 任务ID，失败返回-1
     */
    int download(const std::string& url, const std::string& savePath);
    
    /**
     * 取消下载
     * @param taskId 任务ID
     */
    void cancel(int taskId);
    
    /**
     * 暂停下载
     * @param taskId 任务ID
     */
    void pause(int taskId);
    
    /**
     * 继续下载
     * @param taskId 任务ID
     */
    void resume(int taskId);
    
    // ========== 进度监听 ==========
    
    /**
     * 监听下载进度
     * @param taskId 任务ID
     * @param callback 回调函数
     */
    void onProgress(int taskId, std::function<void(const DownloadProgress&)> callback);
    
    /**
     * 监听下载完成
     * @param taskId 任务ID
     * @param callback 回调函数
     */
    void onCompleted(int taskId, std::function<void()> callback);
    
    /**
     * 监听下载错误
     * @param taskId 任务ID
     * @param callback 回调函数（参数：错误信息）
     */
    void onError(int taskId, std::function<void(const std::string&)> callback);
};
```

### 使用示例

```cpp
// 下载文件
int taskId = DownloadService::instance().download(
    "http://example.com/song.m3u8",
    "/data/cache/song123/index.m3u8"
);

// 监听进度
DownloadService::instance().onProgress(taskId, [](const DownloadProgress& progress) {
    Logger::info("下载进度: " + std::to_string(progress.percent) + "%");
});

// 监听完成
DownloadService::instance().onCompleted(taskId, []() {
    Logger::info("下载完成");
});
```

---

## 📢 事件系统（Event Queue + Event Dispatcher）

> **⚠️ 重要说明**：MVP阶段使用简单的事件模型，不使用订阅/发布模式的EventBus。  
> **相关文档**：[事件模型MVP实现指南（可落地版）.md](./guides/事件模型MVP实现指南（可落地版）.md) ⭐⭐⭐ **必读**  
> **架构说明**：[事件架构规范.md](./architecture/事件架构规范.md) ⭐⭐ **参考**

### 核心组件

**事件系统由以下组件组成：**

1. **EventQueue**：事件队列（`std::queue + mutex + condition_variable`）
2. **EventDispatcher**：事件分发器（运行在Event Loop线程，使用switch路由）
3. **AppEvent**：事件结构（最小化设计，只包含type、arg1、arg2、data）

### 事件定义

```cpp
enum class EventType {
    // 网络事件
    EVENT_CATEGORY_DATA_READY,
    EVENT_SEARCH_RESULT_READY,
    EVENT_SONG_LIST_READY,
    EVENT_NETWORK_ERROR,
    
    // 播放器事件
    EVENT_PLAYER_STATE_CHANGED,
    EVENT_PLAYER_PROGRESS,
    
    // 业务事件
    EVENT_LOGIN_RESULT,
    EVENT_QUEUE_ADD_RESULT,
    // ...
};

struct AppEvent {
    EventType type;
    int arg1 = 0;           // 通用参数1
    int arg2 = 0;           // 通用参数2
    void* data = nullptr;   // 可选数据指针（预分配内存）
};
```

### 使用方式

```cpp
// ✅ 正确：发送事件
AppEvent ev;
ev.type = EventType::EVENT_CATEGORY_DATA_READY;
ev.arg1 = categoryId;
ev.data = categoryData;  // 预分配内存
EventQueue::instance().enqueue(ev);

// ✅ 正确：在EventDispatcher中处理事件（switch路由）
void EventDispatcher::dispatch(const AppEvent& ev) {
    switch (ev.type) {
        case EventType::EVENT_CATEGORY_DATA_READY:
            CategoryService::instance().onDataReady(ev.arg1, ev.data);
            break;
        case EventType::EVENT_SEARCH_RESULT_READY:
            SearchService::instance().onResultReady(ev.data);
            break;
        // ...
    }
}
```

### 核心原则

1. **事件只描述"发生了什么"**：不包含业务逻辑，不包含callback
2. **Service决定"要不要走网络"**：业务判断、缓存策略都在Service层
3. **EventDispatcher只做路由**：switch语句，没有业务逻辑
4. **避免过度设计**：不使用EventBus、订阅系统、反射等

**详细实现说明请参考**：[事件模型MVP实现指南（可落地版）.md](./guides/事件模型MVP实现指南（可落地版）.md)

---

## 📚 HistoryService（历史记录服务）

### 接口定义

```cpp
struct HistoryItem {
    std::string song_id;      // 歌曲ID
    std::string title;        // 歌曲名称
    std::string artist;       // 歌手
    std::string local_path;   // 本地文件路径（可选）
};

class HistoryService {
public:
    static HistoryService& getInstance();
    
    /**
     * 初始化服务
     * @param db_path 数据库文件路径
     * @param max_count 最大记录数（默认 50）
     * @return 0 成功；<0 失败
     */
    int initialize(const std::string& db_path = "/data/ktv_history.db", int max_count = 50);
    
    /**
     * 关闭服务
     */
    void shutdown();
    
    /**
     * 设置容量
     * @param cap 最大记录数
     */
    void setCapacity(size_t cap);
    
    /**
     * 添加历史记录
     * @param item 历史记录项
     * @return 0 成功；<0 失败
     */
    int add(const HistoryItem& item);
    
    /**
     * 获取历史记录列表
     * @param out_items 输出：历史记录列表
     * @return 0 成功；<0 失败
     */
    int getItems(std::vector<HistoryItem>& out_items) const;
    
    /**
     * 清空所有历史记录
     * @return 0 成功；<0 失败
     */
    int clear();
    
    /**
     * 获取记录总数
     * @param out_count 输出：记录总数
     * @return 0 成功；<0 失败
     */
    int getCount(int& out_count) const;
};
```

### 使用示例

```cpp
// 初始化（应用启动时，返回 0 成功，<0 失败）
if (HistoryService::getInstance().initialize("/data/ktv_history.db", 50) != 0) {
    // 初始化失败处理
}

// 播放结束时添加记录
HistoryItem item;
item.song_id = "12345";
item.title = "稻香";
item.artist = "周杰伦";
item.local_path = "/data/cache/song123/index.m3u8";
if (HistoryService::getInstance().add(item) != 0) {
    // 添加失败处理
}

// 获取历史记录列表
std::vector<HistoryItem> history;
if (HistoryService::getInstance().getItems(history) == 0) {
    for (const auto& it : history) {
        // 显示历史记录
    }
}

// 清空历史记录
HistoryService::getInstance().clear();
```

**注意**：
- 使用 SqliteHelper 进行持久化存储（进程唯一 DB）
- 所有函数返回 `int`（0 成功，<0 失败）
- 50/100 条上限，每次插入后自动裁剪
- 详见 [历史记录SQLite实现设计.md](./design/历史记录SQLite实现设计.md)

---

## 📝 LoggingService（日志服务）

### 接口定义

```cpp
enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL
};

class LoggingService {
public:
    static LoggingService& instance();
    
    // ========== 日志输出 ==========
    
    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);
    void fatal(const std::string& message);
    
    // ========== 格式化日志 ==========
    
    template<typename... Args>
    void debug(const char* format, Args... args);
    
    template<typename... Args>
    void info(const char* format, Args... args);
    
    // ... 其他级别类似
    
    // ========== 配置 ==========
    
    /**
     * 设置日志级别
     * @param level 日志级别
     */
    void setLevel(LogLevel level);
    
    /**
     * 设置日志文件路径
     * @param path 文件路径
     */
    void setLogFile(const std::string& path);
};
```

### 使用示例

```cpp
// 简单日志
LoggingService::instance().info("搜索关键词: " + keyword);
LoggingService::instance().error("网络请求失败");

// 格式化日志
LoggingService::instance().info("搜索结果: %d 首", songs.size());
```

---

## 📌 枚举定义

### PlayerState

```cpp
enum class PlayerState {
    IDLE,        // 空闲
    PREPARING,   // 准备中
    PLAYING,     // 播放中
    PAUSED,      // 已暂停
    STOPPED,     // 已停止
    ERROR        // 错误
};
```

---

## 📚 相关文档

- **团队开发规范**：[团队开发规范v1.md](./团队开发规范v1.md)
- **技术基座**：[KTVLV技术基座（F133_Tina）.md](./sdk/KTVLV技术基座（F133_Tina）.md)
- **代码目录结构**：[代码目录结构等于模块责任结构（定版）.md](./guides/代码目录结构等于模块责任结构（定版）.md)
- **NetworkService实现**：[NetworkService与libcurl实现指南（MVP可落地版）.md](./guides/NetworkService与libcurl实现指南（MVP可落地版）.md) ⭐⭐⭐ **必读**
- **事件模型实现**：[事件模型MVP实现指南（可落地版）.md](./guides/事件模型MVP实现指南（可落地版）.md) ⭐⭐⭐ **必读**
- **事件架构规范**：[事件架构规范.md](./architecture/事件架构规范.md) ⭐⭐ **参考**

---

**最后更新**: 2025-12-30  
**状态**: ✅ 核心文档，服务层API设计

