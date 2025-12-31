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

## 🌐 HttpService（HTTP服务）

### 接口定义

```cpp
struct HttpRequest {
    std::string url;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> params;  // GET参数
    std::string body;  // POST body
};

struct HttpResponse {
    int statusCode;
    std::string body;
    std::map<std::string, std::string> headers;
    bool success;  // true表示成功，false表示失败
    std::string errorMessage;  // 错误信息
};

class HttpService {
public:
    static HttpService& instance();
    
    // ========== GET请求 ==========
    
    /**
     * GET请求（简单版本）
     * @param url 请求URL
     * @param params 查询参数（可选）
     * @return HttpResponse
     */
    HttpResponse get(const std::string& url, 
                     const std::map<std::string, std::string>& params = {});
    
    /**
     * GET请求（完整版本）
     * @param request HttpRequest对象
     * @return HttpResponse
     */
    HttpResponse get(const HttpRequest& request);
    
    // ========== POST请求 ==========
    
    /**
     * POST请求（简单版本）
     * @param url 请求URL
     * @param body POST body（JSON字符串）
     * @param headers 请求头（可选）
     * @return HttpResponse
     */
    HttpResponse post(const std::string& url, 
                      const std::string& body,
                      const std::map<std::string, std::string>& headers = {});
    
    /**
     * POST请求（完整版本）
     * @param request HttpRequest对象
     * @return HttpResponse
     */
    HttpResponse post(const HttpRequest& request);
    
    // ========== 其他HTTP方法 ==========
    
    HttpResponse put(const HttpRequest& request);
    HttpResponse del(const HttpRequest& request);  // DELETE
    
    // ========== 配置 ==========
    
    /**
     * 设置默认请求头
     * @param headers 请求头
     */
    void setDefaultHeaders(const std::map<std::string, std::string>& headers);
    
    /**
     * 设置超时时间（秒）
     * @param timeout 超时时间
     */
    void setTimeout(int timeout);
};
```

### 使用示例

```cpp
// GET请求
auto response = HttpService::instance().get("/api/search", {
    {"q", "周杰伦"},
    {"page", "1"},
    {"size", "20"}
});

if (response.success) {
    auto json = Json::parse(response.body);
    // 处理数据
} else {
    Logger::error("请求失败: " + response.errorMessage);
}

// POST请求
std::string jsonBody = R"({"song_id": 12345, "action": "like"})";
auto response = HttpService::instance().post("/api/like", jsonBody, {
    {"Content-Type", "application/json"}
});
```

---

## 🔌 WebSocketService（WebSocket服务）

### 接口定义

```cpp
struct WsMessage {
    std::string type;      // 消息类型（如 "PLAY_SONG", "NEXT"）
    std::string data;      // 消息数据（JSON字符串）
    int timestamp;         // 时间戳
};

class WebSocketService {
public:
    static WebSocketService& instance();
    
    // ========== 连接管理 ==========
    
    /**
     * 连接到WebSocket服务器
     * @param url WebSocket URL（如 "ws://example.com/ws"）
     */
    void connect(const std::string& url);
    
    /**
     * 断开连接
     */
    void disconnect();
    
    /**
     * 是否已连接
     * @return true=已连接, false=未连接
     */
    bool isConnected() const;
    
    // ========== 发送消息 ==========
    
    /**
     * 发送消息
     * @param type 消息类型
     * @param data 消息数据（JSON字符串）
     */
    void send(const std::string& type, const std::string& data);
    
    /**
     * 发送消息（使用WsMessage对象）
     * @param message 消息对象
     */
    void send(const WsMessage& message);
    
    // ========== 事件监听 ==========
    
    /**
     * 监听接收到的消息
     * @param callback 回调函数
     */
    void onMessage(std::function<void(const WsMessage&)> callback);
    
    /**
     * 监听连接建立
     * @param callback 回调函数
     */
    void onConnected(std::function<void()> callback);
    
    /**
     * 监听连接断开
     * @param callback 回调函数
     */
    void onDisconnected(std::function<void()> callback);
    
    /**
     * 监听连接错误
     * @param callback 回调函数（参数：错误信息）
     */
    void onError(std::function<void(const std::string&)> callback);
};
```

### 使用示例

```cpp
// 连接WebSocket
WebSocketService::instance().connect("ws://example.com/ws/room123");

// 监听消息
WebSocketService::instance().onMessage([](const WsMessage& msg) {
    if (msg.type == "PLAY_SONG") {
        auto data = Json::parse(msg.data);
        PlayerService::instance().play(data["url"]);
    } else if (msg.type == "NEXT") {
        PlayerService::instance().next();
    }
});

// 发送消息
WebSocketService::instance().send("LIKE_SONG", R"({"song_id": 12345})");

// 监听连接状态
WebSocketService::instance().onConnected([]() {
    Logger::info("WebSocket已连接");
});

WebSocketService::instance().onDisconnected([]() {
    Logger::warn("WebSocket已断开");
});
```

---

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

## 📢 UiEventBus（UI事件总线）

### 接口定义

```cpp
class UiEventBus {
public:
    static UiEventBus& instance();
    
    // ========== 发送事件 ==========
    
    /**
     * 发送事件（通用版本）
     * @param eventName 事件名称
     * @param data 事件数据（JSON字符串）
     */
    void post(const std::string& eventName, const std::string& data);
    
    /**
     * 发送事件（模板版本，自动序列化）
     * @param eventName 事件名称
     * @param data 事件数据（支持任意可序列化类型）
     */
    template<typename T>
    void post(const std::string& eventName, const T& data);
    
    // ========== 订阅事件 ==========
    
    /**
     * 订阅事件
     * @param eventName 事件名称
     * @param callback 回调函数（参数：JSON字符串）
     */
    void subscribe(const std::string& eventName, 
                   std::function<void(const std::string&)> callback);
    
    /**
     * 取消订阅
     * @param eventName 事件名称
     */
    void unsubscribe(const std::string& eventName);
};
```

### 使用示例

```cpp
// 发送事件
UiEventBus::instance().post("search_result_update", songsJson);
UiEventBus::instance().post("player_state_changed", "playing");

// 订阅事件
UiEventBus::instance().subscribe("search_result_update", [](const std::string& data) {
    auto songs = Song::List::fromJson(data);
    // 更新UI
});
```

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
- **技术基座**：[KTVLV技术基座（F133_Tina）.md](./KTVLV技术基座（F133_Tina）.md)
- **项目脚手架**：[项目脚手架结构.md](./项目脚手架结构.md)

---

**最后更新**: 2025-12-30  
**状态**: ✅ 核心文档，服务层API设计

