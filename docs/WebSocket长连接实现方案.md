# WebSocket 长连接实现方案

> **文档版本**：v1.0  
> **最后更新**：2025-12-30  
> **状态**：第一期功能

## 🎯 功能概述

### WebSocket 用途
- **实时控制**：服务器下发实时命令（播放、暂停、切歌等）
- **长连接**：保持与服务器的持久连接，接收实时消息
- **双向通信**：客户端可以接收服务器命令，也可以向服务器发送消息

### 服务器地址
- **WebSocket URL**：`wss://mc.ktv.com.cn/ws/{license}`
- **协议**：WSS（WebSocket Secure，基于TLS）

---

## 📦 库选型

### 推荐方案：libwebsockets ⭐⭐⭐⭐⭐

**优势**：
- ✅ **轻量级**：C库，适合嵌入式平台（F133）
- ✅ **成熟稳定**：GitHub Stars 4,000+，广泛使用
- ✅ **跨平台**：支持Linux、Windows、嵌入式系统
- ✅ **低代码**：API简洁，易于集成
- ✅ **TLS支持**：支持WSS（WebSocket Secure）
- ✅ **单线程友好**：支持单线程模式

**GitHub**：https://github.com/warmcat/libwebsockets

### 备选方案

#### 方案2: libcurl WebSocket（如果版本支持）
- **优势**：项目已使用libcurl，无需额外库
- **劣势**：需要libcurl 7.86+版本，可能F133 SDK不包含
- **状态**：⚠️ 需要验证F133 SDK的libcurl版本

#### 方案3: websocketpp（不推荐）
- **优势**：C++库，API更现代
- **劣势**：依赖boost.asio，体积较大，不适合嵌入式
- **状态**：❌ 不推荐（体积大，依赖多）

---

## 🔧 实现方案

### 方案1: libwebsockets（推荐）⭐

#### 1.1 集成方式

**方式1: FetchContent（推荐）**
```cmake
include(FetchContent)

FetchContent_Declare(
    libwebsockets
    GIT_REPOSITORY https://github.com/warmcat/libwebsockets.git
    GIT_TAG v4.3-stable
)

FetchContent_MakeAvailable(libwebsockets)

target_link_libraries(ktvlv PRIVATE websockets)
```

**方式2: 系统包管理器（Tina Linux）**
```bash
# 如果Tina Linux SDK包含libwebsockets
opkg install libwebsockets
```

**方式3: 交叉编译**
```bash
# 下载源码
git clone https://github.com/warmcat/libwebsockets.git
cd libwebsockets

# 配置（最小化功能，适合嵌入式）
mkdir build && cd build
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-linux-gnueabihf.cmake \
    -DLWS_WITHOUT_TESTAPPS=ON \
    -DLWS_WITHOUT_TEST_PING=ON \
    -DLWS_WITHOUT_TEST_CLIENT=ON \
    -DLWS_WITHOUT_TEST_SERVER=ON \
    -DLWS_WITH_HTTP2=OFF \
    -DLWS_WITH_BUNDLED_ZLIB=OFF

make
make install
```

#### 1.2 接口设计

```cpp
#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <lws_config.h>
#include <libwebsockets.h>
#include <functional>
#include <string>

namespace ktv {

/**
 * WebSocket客户端（单例，预分配版本）
 * 使用libwebsockets实现WebSocket长连接
 */
class WebSocketClient {
public:
    // 消息回调函数类型
    using MessageCallback = std::function<void(const char* data, size_t len)>;
    
    // 连接状态回调函数类型
    using StatusCallback = std::function<void(bool connected)>;
    
    static WebSocketClient& getInstance() {
        static WebSocketClient instance;
        return instance;
    }
    
    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;
    
    // 初始化
    bool initialize();
    
    // 连接WebSocket服务器
    bool connect(const char* url, const char* license);
    
    // 断开连接
    void disconnect();
    
    // 发送消息
    bool sendMessage(const char* data, size_t len);
    
    // 设置消息回调
    void setMessageCallback(MessageCallback callback);
    
    // 设置状态回调
    void setStatusCallback(StatusCallback callback);
    
    // 处理事件（需要在主循环中调用）
    void processEvents();
    
    // 检查连接状态
    bool isConnected() const;
    
    // 清理
    void cleanup();
    
private:
    WebSocketClient() = default;
    ~WebSocketClient() = default;
    
    // libwebsockets回调函数
    static int callback_http(struct lws* wsi, 
                             enum lws_callback_reasons reason,
                             void* user, void* in, size_t len);
    
    static int callback_websocket(struct lws* wsi,
                                  enum lws_callback_reasons reason,
                                  void* user, void* in, size_t len);
    
    // libwebsockets上下文
    struct lws_context* context_;
    
    // WebSocket连接
    struct lws* wsi_;
    
    // 连接状态
    bool connected_;
    
    // 服务器URL
    std::string server_url_;
    
    // 回调函数
    MessageCallback message_callback_;
    StatusCallback status_callback_;
    
    // 发送缓冲区（预分配）
    static constexpr size_t MAX_SEND_BUFFER = 4096;
    char send_buffer_[MAX_SEND_BUFFER];
    size_t send_buffer_len_;
    
    // 禁止动态分配
    void* operator new(size_t) = delete;
    void* operator new[](size_t) = delete;
    void operator delete(void*) = delete;
    void operator delete[](void*) = delete;
};

} // namespace ktv

#endif // WEBSOCKET_CLIENT_H
```

#### 1.3 实现示例

```cpp
#include "websocket_client.h"
#include <cstring>
#include <cstdio>

namespace ktv {

bool WebSocketClient::initialize() {
    // libwebsockets初始化
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols_;  // WebSocket协议
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_VALIDATE_UTF8;
    
    context_ = lws_create_context(&info);
    if (!context_) {
        return false;
    }
    
    connected_ = false;
    send_buffer_len_ = 0;
    
    return true;
}

bool WebSocketClient::connect(const char* url, const char* license) {
    // 构建完整URL: wss://mc.ktv.com.cn/ws/{license}
    char full_url[256] = {0};
    snprintf(full_url, sizeof(full_url), "%s/%s", url, license);
    server_url_ = full_url;
    
    // 创建WebSocket连接
    struct lws_client_connect_info cci;
    memset(&cci, 0, sizeof(cci));
    
    cci.context = context_;
    cci.address = "mc.ktv.com.cn";
    cci.port = 443;  // WSS端口
    cci.path = "/ws/";
    cci.host = "mc.ktv.com.cn";
    cci.origin = "mc.ktv.com.cn";
    cci.protocol = "ws";
    cci.ssl_connection = LCCSCF_USE_SSL;  // 使用SSL/TLS
    
    wsi_ = lws_client_connect_via_info(&cci);
    if (!wsi_) {
        return false;
    }
    
    return true;
}

void WebSocketClient::processEvents() {
    // 处理libwebsockets事件（需要在主循环中调用）
    if (context_) {
        lws_service(context_, 0);
    }
}

int WebSocketClient::callback_websocket(struct lws* wsi,
                                        enum lws_callback_reasons reason,
                                        void* user, void* in, size_t len) {
    WebSocketClient* client = static_cast<WebSocketClient*>(
        lws_wsi_user(wsi)
    );
    
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            // 连接建立
            client->connected_ = true;
            if (client->status_callback_) {
                client->status_callback_(true);
            }
            break;
            
        case LWS_CALLBACK_CLIENT_RECEIVE:
            // 接收消息
            if (client->message_callback_) {
                client->message_callback_(static_cast<const char*>(in), len);
            }
            break;
            
        case LWS_CALLBACK_CLIENT_CLOSED:
            // 连接关闭
            client->connected_ = false;
            if (client->status_callback_) {
                client->status_callback_(false);
            }
            break;
            
        default:
            break;
    }
    
    return 0;
}

bool WebSocketClient::sendMessage(const char* data, size_t len) {
    if (!connected_ || !wsi_) {
        return false;
    }
    
    // 检查缓冲区大小
    if (len > MAX_SEND_BUFFER) {
        return false;
    }
    
    // 复制到发送缓冲区
    memcpy(send_buffer_, data, len);
    send_buffer_len_ = len;
    
    // 发送消息
    int ret = lws_write(wsi_, 
                        reinterpret_cast<unsigned char*>(send_buffer_),
                        send_buffer_len_,
                        LWS_WRITE_TEXT);
    
    return (ret >= 0);
}

void WebSocketClient::cleanup() {
    if (wsi_) {
        lws_close_reason(wsi_, LWS_CLOSE_STATUS_NORMAL, nullptr, 0);
        wsi_ = nullptr;
    }
    
    if (context_) {
        lws_context_destroy(context_);
        context_ = nullptr;
    }
    
    connected_ = false;
}

} // namespace ktv
```

#### 1.4 使用示例

```cpp
#include "websocket_client.h"

// 初始化WebSocket客户端
void initWebSocket() {
    WebSocketClient::getInstance().initialize();
    
    // 设置消息回调
    WebSocketClient::getInstance().setMessageCallback(
        [](const char* data, size_t len) {
            // 解析服务器下发的JSON命令
            cJSON* json = cJSON_ParseWithLength(data, len);
            if (json) {
                cJSON* cmd = cJSON_GetObjectItem(json, "cmd");
                if (cmd) {
                    const char* cmd_str = cJSON_GetStringValue(cmd);
                    if (strcmp(cmd_str, "play") == 0) {
                        // 处理播放命令
                        handlePlayCommand(json);
                    } else if (strcmp(cmd_str, "pause") == 0) {
                        // 处理暂停命令
                        handlePauseCommand(json);
                    }
                }
                cJSON_Delete(json);
            }
        }
    );
    
    // 设置状态回调
    WebSocketClient::getInstance().setStatusCallback(
        [](bool connected) {
            if (connected) {
                LOG_INFO("WebSocket connected");
            } else {
                LOG_WARN("WebSocket disconnected");
                // 可以尝试重连
            }
        }
    );
    
    // 连接服务器
    const char* license = getLicense();  // 获取license
    WebSocketClient::getInstance().connect(
        "wss://mc.ktv.com.cn/ws",
        license
    );
}

// 在主循环中处理WebSocket事件
void mainLoop() {
    while (running) {
        // 处理WebSocket事件
        WebSocketClient::getInstance().processEvents();
        
        // 处理其他事件
        // ...
        
        // 休眠一段时间
        usleep(10000);  // 10ms
    }
}

// 处理服务器下发的播放命令
void handlePlayCommand(cJSON* json) {
    cJSON* params = cJSON_GetObjectItem(json, "params");
    if (params) {
        cJSON* songid = cJSON_GetObjectItem(params, "songid");
        cJSON* position = cJSON_GetObjectItem(params, "position");
        
        if (songid) {
            int song_id = static_cast<int>(cJSON_GetNumberValue(songid));
            int pos = position ? static_cast<int>(cJSON_GetNumberValue(position)) : 0;
            
            // 调用播放器播放
            TPlayer* player = getPlayer();
            TPlayerSetDataSource(player, getSongUrl(song_id), ...);
            TPlayerPrepare(player);
            TPlayerSeekTo(player, pos);
            TPlayerStart(player);
        }
    }
}
```

---

## 🔌 连接流程

### 1. 初始化流程

```
应用启动
  ↓
初始化WebSocket客户端
  ↓
设置消息回调和状态回调
  ↓
连接服务器（wss://mc.ktv.com.cn/ws/{license}）
  ↓
等待连接建立
  ↓
开始接收实时命令
```

### 2. 连接建立前（服务器要求）

根据接口文档，连接前需要先调用：
```
DELETE /pub?id={license}
```

**实现**：
```cpp
// 连接前清理服务器状态
void prepareWebSocketConnection(const char* license) {
    char url[256] = {0};
    snprintf(url, sizeof(url), "/pub?id=%s", license);
    
    // 使用HttpService发送DELETE请求
    HttpResponse response;
    HttpService::getInstance().deleteRequest(url, response);
    
    // 然后连接WebSocket
    WebSocketClient::getInstance().connect(
        "wss://mc.ktv.com.cn/ws",
        license
    );
}
```

### 3. 消息格式

服务器下发的JSON命令示例：
```json
{
    "cmd": "play",
    "params": {
        "songid": 7588436,
        "position": 0
    }
}
```

---

## 📊 库对比

| 库名 | 语言 | 体积 | 依赖 | 嵌入式支持 | 推荐度 |
|------|------|------|------|-----------|--------|
| **libwebsockets** | C | 小 | 少 | ✅ 优秀 | ⭐⭐⭐⭐⭐ |
| **libcurl WebSocket** | C | 中 | 无（已使用） | ⚠️ 需验证版本 | ⭐⭐⭐ |
| **websocketpp** | C++ | 大 | boost.asio | ❌ 不适合 | ⭐⭐ |

---

## 🔧 编译配置

### CMakeLists.txt 配置

```cmake
# 方式1: FetchContent（推荐）
include(FetchContent)

FetchContent_Declare(
    libwebsockets
    GIT_REPOSITORY https://github.com/warmcat/libwebsockets.git
    GIT_TAG v4.3-stable
)

set(LWS_WITHOUT_TESTAPPS ON CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_PING ON CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_CLIENT ON CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_SERVER ON CACHE BOOL "" FORCE)
set(LWS_WITH_HTTP2 OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(libwebsockets)

target_link_libraries(ktvlv PRIVATE websockets)
```

### F133 平台特殊配置

```cmake
if(KTV_PLATFORM_F133_LINUX)
    # F133平台可能需要链接SSL库（WSS支持）
    find_library(SSL_LIB ssl)
    find_library(CRYPTO_LIB crypto)
    if(SSL_LIB AND CRYPTO_LIB)
        target_link_libraries(ktvlv PRIVATE ${SSL_LIB} ${CRYPTO_LIB})
    endif()
endif()
```

---

## 📝 注意事项

### 1. TLS/SSL支持
- WSS需要TLS/SSL支持
- F133平台需要确保有OpenSSL或mbedTLS库
- libwebsockets支持多种TLS后端

### 2. 单线程设计
- libwebsockets支持单线程模式
- 需要在主循环中调用`lws_service()`
- 避免在回调函数中执行耗时操作

### 3. 重连机制
- 连接断开后需要自动重连
- 建议实现指数退避重连策略
- 避免频繁重连导致服务器压力

### 4. 消息处理
- 服务器下发的消息是JSON格式
- 使用cJSON解析消息
- 根据`cmd`字段分发到不同的处理函数

### 5. 内存管理
- 使用预分配缓冲区
- 避免在回调函数中动态分配内存
- 及时释放cJSON对象

---

## 📚 相关文档

- **HTTP REST API客户端设计**: [HTTP_REST_API客户端设计.md](./design/HTTP_REST_API客户端设计.md)
- **接口调用顺序**: [接口调用顺序.md](./guides/接口调用顺序.md)
- **服务器API清单**: [服务器API_LIST.md](./api/服务器API_LIST.md)
- **开源库选型指南**: [开源库选型指南.md](./guides/开源库选型指南.md)

---

## 🎯 总结

### 推荐方案

**使用 libwebsockets 实现WebSocket长连接**

**原因**：
1. ✅ **轻量级**：适合F133嵌入式平台
2. ✅ **成熟稳定**：GitHub Stars 4,000+，广泛使用
3. ✅ **低代码**：API简洁，易于集成
4. ✅ **TLS支持**：支持WSS（WebSocket Secure）
5. ✅ **单线程友好**：支持单线程模式

### 实现要点

1. ✅ **连接前清理**：先调用`DELETE /pub?id={license}`
2. ✅ **消息回调**：设置消息回调处理服务器命令
3. ✅ **状态回调**：设置状态回调处理连接状态变化
4. ✅ **主循环处理**：在主循环中调用`processEvents()`
5. ✅ **JSON解析**：使用cJSON解析服务器下发的JSON命令

---

**最后更新**: 2025-12-30



