# Android/Web/Python 背景工程师 C++ 开发指南

> **文档版本**：v1.0  
> **最后更新**：2025-12-30  
> **状态**：✅ 核心文档  
> **适用对象**：Android/Java/Web/Python 背景的工程师，刚开始接触 C/C++ Linux 编程  
> **相关文档**：详见 [团队开发规范v1.md](../团队开发规范v1.md)、[资源管理规范v1.md](../资源管理规范v1.md)

---

## 🎯 一句话定位

> **我们写的不是嵌入式系统，是一个运行在 TinaLinux 上的"微型网易云客户端"。**  
> **业务为王，底层靠选型。Cursor AI 混合开发，避免造轮子，提前分配资源，避免资源泄露。**

---

## 📋 目录

1. [团队背景与设计原则](#一团队背景与设计原则)
2. [Java/Android → C++ 映射表](#二javaandroid--c-映射表)
3. [Web → C++ 映射表](#三web--c-映射表)
4. [Python → C++ 映射表](#四python--c-映射表)
5. [核心设计原则（必须遵守）](#五核心设计原则必须遵守)
6. [资源管理（避免泄露）](#六资源管理避免泄露)
7. [线程安全（无锁业务层）](#七线程安全无锁业务层)
8. [调试技巧（容易调试）](#八调试技巧容易调试)
9. [状态机简化（少状态机）](#九状态机简化少状态机)
10. [UI 简化（简化页面和控件）](#十ui-简化简化页面和控件)
11. [Cursor AI 使用指南](#十一cursor-ai-使用指南)
12. [常见陷阱与避坑](#十二常见陷阱与避坑)

---

## 一、团队背景与设计原则

### 1.1 团队背景

| 背景 | 人数 | 主要技能 | 学习曲线 |
|------|------|---------|---------|
| **Android/Java** | 多数 | Java、Kotlin、Android SDK | ⭐⭐⭐ 中等（2-3周） |
| **Web** | 部分 | JavaScript、TypeScript、React/Vue | ⭐⭐⭐ 中等（2-3周） |
| **Python 服务器** | 部分 | Python、Django/Flask、REST API | ⭐⭐⭐ 中等（2-3周） |

### 1.2 设计原则（针对团队背景）

| 原则 | 原因 | 实现方式 |
|------|------|---------|
| **避免造轮子** | 团队不熟悉底层实现 | 优先使用开源库（libcurl、cJSON、libwebsockets） |
| **提前分配资源** | 避免运行时内存分配失败 | 所有资源 Singleton，启动时预分配 |
| **避免资源泄露** | C++ 没有 GC，容易泄露 | 禁止 `new/delete`，使用 Singleton + 预分配 |
| **无锁数据结构** | 避免死锁和竞态条件 | 业务层无锁，使用 `std::queue + std::mutex`（框架层） |
| **线程安全** | 多线程容易出错 | 业务层单线程，跨线程用事件队列 |
| **容易调试** | 嵌入式调试困难 | 统一日志、状态可视化、错误提示 |
| **少状态机** | 状态机容易出错 | 使用简单枚举，避免复杂状态转换 |
| **简化页面和控件** | UI 复杂度高 | 预创建控件，使用 `show()/hide()` 切换 |

---

## 二、Java/Android → C++ 映射表

### 2.1 基础概念映射

| Java/Android | C++ (本项目) | 说明 |
|-------------|-------------|------|
| `Activity` | `PageXxx` (Singleton) | 页面，使用 `show()/hide()` 切换 |
| `Fragment` | `XxxView` (预创建控件) | UI 组件，预创建不销毁 |
| `RecyclerView` | `lv_list` + 控件池 | 列表，预创建固定数量 item |
| `Handler.post()` | `UiDispatcher::post()` | UI 线程任务调度 |
| `LiveData` | `UiEventQueue` | 事件队列，跨线程通知 |
| `Retrofit` | `HttpService` (Singleton) | HTTP 客户端，封装 libcurl |
| `OkHttp` | `HttpService` (Singleton) | HTTP 客户端，封装 libcurl |
| `Gson` | `JsonHelper` (封装 cJSON) | JSON 解析，封装 cJSON |
| `WebSocket` | `WebSocketService` (Singleton) | WebSocket 客户端，封装 libwebsockets |
| `SharedPreferences` | `CacheService` (Singleton) | 本地存储，使用 inih |
| `Thread` | `std::thread` (框架层) | 线程，业务层不直接创建 |
| `ExecutorService` | Service 线程池 | 线程池，框架层封装 |
| `synchronized` | `std::mutex` (框架层) | 锁，业务层无锁 |
| `volatile` | `std::atomic` (框架层) | 原子变量，框架层封装 |

### 2.2 代码风格映射

#### Java/Android 风格
```java
// ❌ Java 风格（不要这样写）
public class SearchActivity extends Activity {
    private RecyclerView recyclerView;
    private List<Song> songs = new ArrayList<>();
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        recyclerView = findViewById(R.id.recycler_view);
        // ...
    }
}
```

#### C++ 风格（本项目）
```cpp
// ✅ C++ 风格（本项目）
class SearchPage {
public:
    static SearchPage& instance();  // Singleton
    
    void show();   // 显示页面
    void hide();   // 隐藏页面
    void update(const SongList& songs);  // 更新数据
    
private:
    SearchPage();  // 私有构造，Singleton
    lv_obj_t* list_;  // 预创建的列表控件
    SongItem items_[MAX_SONGS];  // 预分配的 item 数组
};
```

---

## 三、Web → C++ 映射表

### 3.1 基础概念映射

| Web | C++ (本项目) | 说明 |
|-----|-------------|------|
| `React Component` | `PageXxx` (Singleton) | 页面组件，使用 `show()/hide()` 切换 |
| `useState` | `update()` 方法 | 状态更新，调用 `update()` 刷新 UI |
| `useEffect` | `onShow()` / `onHide()` | 生命周期，页面显示/隐藏时调用 |
| `fetch()` | `HttpService::get()` / `post()` | HTTP 请求，封装 libcurl |
| `WebSocket` | `WebSocketService` (Singleton) | WebSocket 客户端，封装 libwebsockets |
| `localStorage` | `CacheService` (Singleton) | 本地存储，使用 inih |
| `JSON.parse()` | `JsonHelper::parse()` | JSON 解析，封装 cJSON |
| `Promise` | `std::async` (框架层) | 异步任务，框架层封装 |
| `EventEmitter` | `UiEventBus` (Singleton) | 事件总线，跨模块通信 |

### 3.2 代码风格映射

#### Web 风格
```javascript
// ❌ Web 风格（不要这样写）
function SearchPage() {
    const [songs, setSongs] = useState([]);
    
    useEffect(() => {
        fetch('/api/songs/search?q=test')
            .then(res => res.json())
            .then(data => setSongs(data.items));
    }, []);
    
    return <div>{songs.map(song => <SongItem key={song.id} />)}</div>;
}
```

#### C++ 风格（本项目）
```cpp
// ✅ C++ 风格（本项目）
class SearchPage {
public:
    static SearchPage& instance();  // Singleton
    
    void show() {
        // 显示页面
        lv_obj_set_hidden(container_, false);
        // 触发搜索（通过 HttpService）
        HttpService::instance().get("/api/songs/search?q=test", 
            [this](const HttpResponse& res) {
                SongList list;
                if (JsonHelper::parse(res.body, &list)) {
                    this->update(list);  // 更新 UI
                }
            });
    }
    
    void update(const SongList& songs) {
        // 更新预创建的 item
        for (int i = 0; i < songs.count && i < MAX_SONGS; ++i) {
            items_[i].setData(songs.items[i]);
            lv_obj_set_hidden(items_[i].getObj(), false);
        }
    }
    
private:
    SearchPage();  // 私有构造，Singleton
    lv_obj_t* container_;  // 预创建的容器
    SongItem items_[MAX_SONGS];  // 预分配的 item 数组
};
```

---

## 四、Python → C++ 映射表

### 4.1 基础概念映射

| Python | C++ (本项目) | 说明 |
|--------|-------------|------|
| `class` | `class` (Singleton) | 类，使用 Singleton 模式 |
| `dict` | `struct` + `JsonHelper` | 字典，使用 struct + JSON 解析 |
| `list` | `std::array` (固定大小) | 列表，使用固定大小数组 |
| `requests.get()` | `HttpService::get()` | HTTP 请求，封装 libcurl |
| `websocket` | `WebSocketService` (Singleton) | WebSocket 客户端，封装 libwebsockets |
| `json.loads()` | `JsonHelper::parse()` | JSON 解析，封装 cJSON |
| `threading.Thread` | `std::thread` (框架层) | 线程，框架层封装 |
| `queue.Queue` | `std::queue + std::mutex` (框架层) | 队列，框架层封装 |
| `@singleton` | `static instance()` | 单例模式，使用 `instance()` 方法 |

### 4.2 代码风格映射

#### Python 风格
```python
# ❌ Python 风格（不要这样写）
class SearchPage:
    def __init__(self):
        self.songs = []
    
    def load_songs(self, query):
        response = requests.get(f'/api/songs/search?q={query}')
        data = json.loads(response.text)
        self.songs = data['items']
        self.update_ui()
```

#### C++ 风格（本项目）
```cpp
// ✅ C++ 风格（本项目）
class SearchPage {
public:
    static SearchPage& instance();  // Singleton
    
    void loadSongs(const std::string& query) {
        std::string url = "/api/songs/search?q=" + query;
        HttpService::instance().get(url, [this](const HttpResponse& res) {
            SongList list;
            if (JsonHelper::parse(res.body, &list)) {
                this->update(list);  // 更新 UI
            }
        });
    }
    
    void update(const SongList& songs) {
        // 更新预创建的 item
        for (int i = 0; i < songs.count && i < MAX_SONGS; ++i) {
            items_[i].setData(songs.items[i]);
        }
    }
    
private:
    SearchPage();  // 私有构造，Singleton
    SongItem items_[MAX_SONGS];  // 预分配的 item 数组
};
```

---

## 五、核心设计原则（必须遵守）

### 5.1 避免造轮子

> **原则**：优先使用成熟的开源库，不要自己实现。

| 功能 | 开源库 | 替代方案 |
|------|--------|---------|
| HTTP 客户端 | `libcurl` (40,000+ stars) | ❌ 不要自己实现 HTTP |
| JSON 解析 | `cJSON` (10,000+ stars) | ❌ 不要自己解析 JSON |
| WebSocket | `libwebsockets` (4,000+ stars) | ❌ 不要自己实现 WebSocket |
| 配置文件 | `inih` (1,000+ stars) | ❌ 不要自己解析 INI |
| UI 框架 | `LVGL` (15,000+ stars) | ❌ 不要自己实现 UI |

**示例**：
```cpp
// ❌ 错误：自己实现 HTTP
void fetchSongs() {
    // 自己写 socket、TLS、HTTP 协议...
}

// ✅ 正确：使用 libcurl（通过 HttpService）
void fetchSongs() {
    HttpService::instance().get("/api/songs", [](const HttpResponse& res) {
        // 处理响应
    });
}
```

### 5.2 提前分配资源

> **原则**：所有资源在启动时预分配，运行时不再分配。

| 资源类型 | 预分配方式 | 生命周期 |
|---------|-----------|---------|
| **页面** | Singleton，启动时创建 | App 生命周期 |
| **UI 控件** | 页面构建时预创建 | 页面生命周期 |
| **列表 Item** | 固定数量数组（如 50 个） | 页面生命周期 |
| **HTTP 连接** | `HttpService` Singleton | App 生命周期 |
| **WebSocket** | `WebSocketService` Singleton | App 生命周期 |
| **缓存** | `CacheService` Singleton | App 生命周期 |

**示例**：
```cpp
// ❌ 错误：运行时动态创建
void showSearchPage() {
    lv_obj_t* list = lv_list_create(lv_scr_act());  // 每次创建
    // ...
}

// ✅ 正确：预创建，使用 show()/hide()
class SearchPage {
private:
    lv_obj_t* list_;  // 预创建
    SongItem items_[50];  // 预分配 50 个 item
    
public:
    SearchPage() {
        list_ = lv_list_create(lv_scr_act());  // 构造时创建
        for (int i = 0; i < 50; ++i) {
            items_[i].create(list_);  // 预创建 item
        }
    }
    
    void show() {
        lv_obj_set_hidden(list_, false);  // 显示
    }
    
    void hide() {
        lv_obj_set_hidden(list_, true);  // 隐藏
    }
};
```

### 5.3 避免资源泄露

> **原则**：禁止 `new/delete`，使用 Singleton + 预分配。

| 操作 | Java/Android | C++ (本项目) |
|------|-------------|-------------|
| **创建对象** | `new MyClass()` | `MyClass::instance()` (Singleton) |
| **删除对象** | GC 自动回收 | ❌ 不删除（Singleton，App 生命周期） |
| **创建控件** | `new View()` | 预创建，使用 `show()/hide()` |
| **删除控件** | `removeView()` | ❌ 不删除（预创建，页面生命周期） |

**示例**：
```cpp
// ❌ 错误：动态分配
void showSearchPage() {
    SearchPage* page = new SearchPage();  // 泄露！
    // ...
}

// ✅ 正确：Singleton
void showSearchPage() {
    SearchPage::instance().show();  // Singleton，不泄露
}
```

### 5.4 无锁数据结构

> **原则**：业务层无锁，框架层使用 `std::queue + std::mutex`。

| 层级 | 锁的使用 | 说明 |
|------|---------|------|
| **业务层** | ❌ 无锁 | 业务代码不接触锁 |
| **框架层** | ✅ `std::queue + std::mutex` | 框架层封装，业务层不感知 |

**示例**：
```cpp
// ❌ 错误：业务层使用锁
class SearchPage {
private:
    std::mutex mtx_;  // 业务层不应该有锁
    
public:
    void update(const SongList& songs) {
        std::lock_guard<std::mutex> lock(mtx_);  // 业务层不应该锁
        // ...
    }
};

// ✅ 正确：业务层无锁，框架层封装
class SearchPage {
public:
    void update(const SongList& songs) {
        // 业务层无锁，直接更新 UI（UI 线程）
        for (int i = 0; i < songs.count; ++i) {
            items_[i].setData(songs.items[i]);
        }
    }
};

// 框架层（HttpService）使用 std::queue + std::mutex
class HttpService {
private:
    std::queue<HttpRequest> queue_;
    std::mutex mtx_;  // 框架层有锁，业务层不感知
};
```

### 5.5 线程安全

> **原则**：业务层单线程，跨线程用事件队列。

| 线程 | 职责 | 业务层接触 |
|------|------|-----------|
| **UI 线程** | LVGL 渲染、UI 更新 | ✅ 业务层主要在这里 |
| **Player 线程** | 播放器控制 | ❌ 业务层不接触 |
| **Network 线程** | HTTP/WebSocket | ❌ 业务层不接触 |
| **SDK 内部线程** | TPlayer 内部 | ❌ 业务层不接触 |

**示例**：
```cpp
// ❌ 错误：跨线程直接更新 UI
void onHttpResponse(const HttpResponse& res) {
    // Network 线程
    lv_label_set_text(label_, res.body.c_str());  // 错误！跨线程更新 UI
}

// ✅ 正确：通过事件队列
void onHttpResponse(const HttpResponse& res) {
    // Network 线程
    UiEventQueue::push(SongListEvent{res.body});  // 推送到事件队列
}

// UI 线程消费事件
void onEvent(const SongListEvent& event) {
    // UI 线程
    SearchPage::instance().update(event.songs);  // 安全更新 UI
}
```

---

## 六、资源管理（避免泄露）

### 6.1 资源清单（全面 Singleton）

| 资源类型 | 单例类/组件 | 创建时机 | 生命周期 | 替代行为 |
|---------|-----------|---------|---------|---------|
| **播放器 SDK** | `PlayerService` | App 启动 | 全局唯一 | 禁直接调 tplayer |
| **WebSocket** | `WebSocketService` | 进入点歌房间前 | 全局唯一/可重连 | 用 `connect()/disconnect()` |
| **HTTP 客户端** | `HttpService` | App 启动 | 全局唯一 | 复用连接 |
| **缓存管理** | `CacheService` | App 启动 | 全局唯一 | 避免重复落盘 |
| **日志系统** | `Logger` | App 启动 | 全局唯一 | 不允许多 init |
| **事件总线** | `UiEventBus` | App 启动 | 全局唯一 | 控制 UI 安全调用 |
| **根页面容器** | `AppUIRoot` | LVGL init 后 | 全局唯一 | 页面挂载 |
| **页面（每个 Page）** | `PageXxx` | 首次进入时 | 保持不销毁 | `show()/hide()` 切换 |
| **控件容器** | `XxxView` | 页面构建时 | 保持不销毁 | `update()/reset()` |
| **线程/队列** | 内置在 Service | Service 初始化时 | 稳定运行到退出 | 不让业务操作 |

### 6.2 UI 处理策略

#### ❌ 错误写法（会导致泄露 + 碎片）
```cpp
void showSearchPage() {
    lv_obj_t* list = lv_list_create(lv_scr_act());  // 每次创建
    for (int i = 0; i < 100; ++i) {
        lv_obj_t* item = lv_list_add_btn(list, NULL, "Song");  // 每次创建
    }
}

void hideSearchPage() {
    lv_obj_del(list);  // 删除，导致碎片
}
```

#### ✅ 正确写法（预创建 + show/hide）
```cpp
class SearchPage {
private:
    lv_obj_t* list_;  // 预创建
    SongItem items_[50];  // 预分配 50 个 item
    
public:
    SearchPage() {
        list_ = lv_list_create(lv_scr_act());  // 构造时创建
        for (int i = 0; i < 50; ++i) {
            items_[i].create(list_);  // 预创建 item
            lv_obj_set_hidden(items_[i].getObj(), true);  // 初始隐藏
        }
    }
    
    void show() {
        lv_obj_set_hidden(list_, false);  // 显示
    }
    
    void hide() {
        lv_obj_set_hidden(list_, true);  // 隐藏（不删除）
    }
    
    void update(const SongList& songs) {
        // 更新预创建的 item
        for (int i = 0; i < songs.count && i < 50; ++i) {
            items_[i].setData(songs.items[i]);
            lv_obj_set_hidden(items_[i].getObj(), false);
        }
        // 隐藏多余的 item
        for (int i = songs.count; i < 50; ++i) {
            lv_obj_set_hidden(items_[i].getObj(), true);
        }
    }
};
```

---

## 七、线程安全（无锁业务层）

### 7.1 线程架构

```
┌─────────────────────────────────────────────────────────┐
│ UI 线程（主线程，LVGL）                                   │
│ - 业务层主要在这里                                        │
│ - 无锁，直接更新 UI                                       │
└─────────────────────────────────────────────────────────┘
         ↑                    ↓
    UiEventQueue      PlayerCmdQueue
         ↑                    ↓
┌─────────────────────────────────────────────────────────┐
│ Player 线程（std::thread）                                │
│ - 播放器控制                                             │
│ - 框架层封装，业务层不接触                                │
└─────────────────────────────────────────────────────────┘
         ↑                    ↓
    UiEventQueue      HttpRequestQueue
         ↑                    ↓
┌─────────────────────────────────────────────────────────┐
│ Network 线程（std::async）                                │
│ - HTTP/WebSocket                                         │
│ - 框架层封装，业务层不接触                                │
└─────────────────────────────────────────────────────────┘
```

### 7.2 业务层无锁原则

| 操作 | 业务层 | 框架层 |
|------|-------|--------|
| **更新 UI** | 直接更新（UI 线程） | 事件队列（跨线程） |
| **发送命令** | `PlayerService::play()` | `PlayerCmdQueue`（框架层） |
| **接收事件** | `UiEventBus::subscribe()` | `UiEventQueue`（框架层） |
| **HTTP 请求** | `HttpService::get()` | `HttpRequestQueue`（框架层） |

**示例**：
```cpp
// ✅ 正确：业务层无锁
class SearchPage {
public:
    void update(const SongList& songs) {
        // UI 线程，直接更新（无锁）
        for (int i = 0; i < songs.count; ++i) {
            items_[i].setData(songs.items[i]);
        }
    }
    
    void onSearchClick() {
        // UI 线程，发送 HTTP 请求（框架层封装，无锁）
        HttpService::instance().get("/api/songs/search?q=test",
            [this](const HttpResponse& res) {
                // 回调在 UI 线程，直接更新（无锁）
                SongList list;
                if (JsonHelper::parse(res.body, &list)) {
                    this->update(list);
                }
            });
    }
};
```

---

## 八、调试技巧（容易调试）

### 8.1 统一日志

> **原则**：使用 `syslog`（F133）统一日志，格式：`[ktv][module][event] key=value`

| 级别 | 使用场景 | 示例 |
|------|---------|------|
| **LOG_ERR** | 播放失败、解码失败 | `syslog(LOG_ERR, "[ktv][player][fail] song_id=%s err=%d", id, err);` |
| **LOG_WARNING** | 网络慢/超时重试 | `syslog(LOG_WARNING, "[ktv][net][timeout] path=%s retry=%d", path, retry);` |
| **LOG_INFO** | 状态变化、关键动作 | `syslog(LOG_INFO, "[ktv][ui][page_enter] page=search");` |
| **LOG_DEBUG** | 仅开发调试 | `syslog(LOG_DEBUG, "[ktv][search][query] q=%s", query);` |

**示例**：
```cpp
// ✅ 正确：统一日志格式
void SearchPage::loadSongs(const std::string& query) {
    syslog(LOG_INFO, "[ktv][search][start] query=%s", query.c_str());
    
    HttpService::instance().get("/api/songs/search?q=" + query,
        [this, query](const HttpResponse& res) {
            if (res.status_code == 200) {
                syslog(LOG_INFO, "[ktv][search][ok] query=%s count=%d", 
                    query.c_str(), res.body.size());
                // ...
            } else {
                syslog(LOG_ERR, "[ktv][search][fail] query=%s code=%d", 
                    query.c_str(), res.status_code);
            }
        });
}
```

### 8.2 状态可视化

> **原则**：关键状态用 UI 显示，方便调试。

**示例**：
```cpp
class SearchPage {
private:
    lv_obj_t* status_label_;  // 状态标签
    
public:
    void setStatus(const std::string& status) {
        lv_label_set_text(status_label_, status.c_str());
    }
    
    void loadSongs(const std::string& query) {
        setStatus("搜索中...");
        HttpService::instance().get("/api/songs/search?q=" + query,
            [this](const HttpResponse& res) {
                if (res.status_code == 200) {
                    setStatus("搜索成功");
                    // ...
                } else {
                    setStatus("搜索失败: " + std::to_string(res.status_code));
                }
            });
    }
};
```

### 8.3 错误提示

> **原则**：错误信息清晰，包含上下文。

**示例**：
```cpp
// ❌ 错误：错误信息不清晰
if (!parseJson(json, &list)) {
    syslog(LOG_ERR, "parse failed");
}

// ✅ 正确：错误信息包含上下文
if (!JsonHelper::parse(json, &list)) {
    syslog(LOG_ERR, "[ktv][search][json_fail] query=%s json_len=%zu", 
        query.c_str(), json.size());
    setStatus("解析失败，请重试");
}
```

---

## 九、状态机简化（少状态机）

### 9.1 简单枚举 vs 复杂状态机

| 场景 | 复杂状态机 | 简单枚举（推荐） |
|------|-----------|----------------|
| **播放器状态** | `IDLE → LOADING → PLAYING → PAUSED → ERROR` | `enum PlayerState { IDLE, PLAYING, PAUSED, ERROR };` |
| **页面状态** | `INIT → LOADING → READY → ERROR → RETRY` | `enum PageState { LOADING, READY, ERROR };` |
| **网络状态** | `DISCONNECTED → CONNECTING → CONNECTED → RECONNECTING` | `enum NetState { DISCONNECTED, CONNECTED };` |

**示例**：
```cpp
// ❌ 错误：复杂状态机
class PlayerService {
    enum State {
        IDLE, LOADING, BUFFERING, PLAYING, PAUSED, 
        STOPPING, ERROR, RECOVERING
    };
    State state_ = IDLE;
    
    void play() {
        switch (state_) {
            case IDLE: state_ = LOADING; break;
            case PAUSED: state_ = PLAYING; break;
            case ERROR: state_ = RECOVERING; break;
            // ... 复杂的状态转换
        }
    }
};

// ✅ 正确：简单枚举
class PlayerService {
    enum State {
        IDLE,
        PLAYING,
        PAUSED,
        ERROR
    };
    State state_ = IDLE;
    
    void play() {
        if (state_ == IDLE || state_ == ERROR) {
            // 开始播放
            state_ = PLAYING;
        } else if (state_ == PAUSED) {
            // 继续播放
            state_ = PLAYING;
        }
    }
};
```

### 9.2 避免状态机的最佳实践

| 原则 | 实现方式 |
|------|---------|
| **使用简单枚举** | `enum State { A, B, C };` |
| **避免状态转换表** | 直接 if/else，不用状态机表 |
| **状态与 UI 分离** | 状态存数据，UI 根据数据渲染 |
| **避免嵌套状态** | 扁平化状态，不用子状态 |

---

## 十、UI 简化（简化页面和控件）

### 10.1 页面简化

| 原则 | 实现方式 |
|------|---------|
| **预创建页面** | Singleton，启动时创建 |
| **使用 show/hide** | 不删除页面，只切换显示 |
| **固定布局** | 不使用动态布局 |
| **简化动画** | 避免复杂动画 |

**示例**：
```cpp
// ✅ 正确：简化页面
class SearchPage {
public:
    static SearchPage& instance() {
        static SearchPage inst;
        return inst;
    }
    
    void show() {
        lv_obj_set_hidden(container_, false);
    }
    
    void hide() {
        lv_obj_set_hidden(container_, true);
    }
    
private:
    SearchPage() {
        container_ = lv_obj_create(lv_scr_act());
        // 预创建所有控件
        createSearchBar();
        createSongList();
        createStatusLabel();
    }
    
    lv_obj_t* container_;
    lv_obj_t* search_bar_;
    lv_obj_t* song_list_;
    lv_obj_t* status_label_;
};
```

### 10.2 控件简化

| 原则 | 实现方式 |
|------|---------|
| **预创建控件** | 页面构建时预创建 |
| **固定数量** | 列表 item 固定数量（如 50 个） |
| **使用 show/hide** | 不删除控件，只切换显示 |
| **简化样式** | 使用统一主题，避免复杂样式 |

**示例**：
```cpp
// ✅ 正确：简化控件
class SongList {
private:
    SongItem items_[50];  // 预分配 50 个 item
    
public:
    SongList() {
        for (int i = 0; i < 50; ++i) {
            items_[i].create(parent_);  // 预创建
            lv_obj_set_hidden(items_[i].getObj(), true);  // 初始隐藏
        }
    }
    
    void update(const SongList& songs) {
        // 显示需要的 item
        for (int i = 0; i < songs.count && i < 50; ++i) {
            items_[i].setData(songs.items[i]);
            lv_obj_set_hidden(items_[i].getObj(), false);
        }
        // 隐藏多余的 item
        for (int i = songs.count; i < 50; ++i) {
            lv_obj_set_hidden(items_[i].getObj(), true);
        }
    }
};
```

---

## 十一、Cursor AI 使用指南

### 11.1 Cursor AI 提示模板

> **复制以下内容给 Cursor AI，生成符合规范的代码**

```
We are implementing a KTVLV project (F133/Tina Linux) with the following architecture:

**Team Background:**
- Engineers: Android/Java, Web, Python server background
- New to C/C++ Linux programming
- Most work will be done with Cursor AI assistance

**Resource Management Principles:**
- ALL resources are Singleton (no new/delete/free)
- UI controls created once, never deleted (use show()/hide())
- Lifecycle = App lifecycle (no manual release)
- No malloc/free/new/delete in business code
- Pre-allocate all resources at startup

**Architecture:**
- 4 threads: UI (main), PlayerThread (std::thread), Network (std::async), SDK internal thread
- 2 message queues: PlayerCmdQueue (UI/Network -> PlayerThread), UiEventQueue (PlayerThread/Network -> UI)
- Service layer: PlayerService, HttpService, WebSocketService, CacheService, etc. (all Singleton)
- Business layer: features/ (Search, Charts, Playlist, etc.) - Java/Web style development

**Rules:**
- Command Down / Event Up
- UI never calls tplayer directly (use PlayerService)
- tplayer callbacks push events to UiEventQueue, then UiDispatcher::post() to UI thread
- std::queue + mutex inside services, no lock exposed to business layer
- No moodycamel, no boost, no raw pthread for business
- No direct cross-thread widget updates
- No new/delete/free/lv_obj_del in business code
- All Pages are Singleton (created once, use show()/hide())
- All Services are Singleton (created once, lifecycle = App lifecycle)
- Avoid wheel reinvention: use libcurl, cJSON, libwebsockets, inih
- Simple state machine: use enum, avoid complex state transitions
- Easy debugging: use syslog with [ktv][module][event] format
- Simplify UI: pre-create controls, use show()/hide()

**Forbidden:**
- ❌ new/delete/free/lv_obj_del in business code
- ❌ Creating controls in loops (use control pool)
- ❌ Creating pages on each navigation (use Singleton + show()/hide())
- ❌ Direct tplayer_* calls (use PlayerService)
- ❌ curl_easy_* in business code (use HttpService)
- ❌ pthread_create in business code (use Service threads)
- ❌ Complex state machines (use simple enum)
- ❌ Manual memory management (use Singleton + pre-allocation)

**Patterns:**
- Singleton pattern for all Services and Pages
- Control pool for lists (pre-create fixed number of items)
- show()/hide() for page navigation
- update() for data refresh (don't recreate controls)
- Java/Web style: HttpService::instance().get() like Retrofit/axios
- Python style: JsonHelper::parse() like json.loads()
```

### 11.2 Cursor AI 使用技巧

| 技巧 | 说明 |
|------|------|
| **提供上下文** | 复制相关代码和文档链接 |
| **明确要求** | 明确说明"使用 Singleton"、"预创建控件"等 |
| **检查生成代码** | 检查是否有 `new/delete`、是否有锁等 |
| **迭代优化** | 多次对话，逐步优化代码 |

---

## 十二、常见陷阱与避坑

### 12.1 内存泄露陷阱

| 陷阱 | 错误代码 | 正确代码 |
|------|---------|---------|
| **动态创建页面** | `new SearchPage()` | `SearchPage::instance()` |
| **动态创建控件** | `lv_obj_create()` 在循环中 | 预创建，使用 `show()/hide()` |
| **忘记释放** | `lv_obj_del(obj)` | ❌ 不删除，使用 `show()/hide()` |

### 12.2 线程安全陷阱

| 陷阱 | 错误代码 | 正确代码 |
|------|---------|---------|
| **跨线程更新 UI** | Network 线程直接 `lv_label_set_text()` | 通过 `UiEventQueue` + `UiDispatcher` |
| **业务层使用锁** | `std::mutex` 在业务类中 | 框架层封装，业务层无锁 |

### 12.3 状态机陷阱

| 陷阱 | 错误代码 | 正确代码 |
|------|---------|---------|
| **复杂状态机** | 状态转换表、嵌套状态 | 简单枚举，直接 if/else |

### 12.4 UI 简化陷阱

| 陷阱 | 错误代码 | 正确代码 |
|------|---------|---------|
| **动态创建控件** | 循环中 `lv_obj_create()` | 预创建固定数量，使用 `show()/hide()` |
| **复杂动画** | 复杂动画效果 | 简化动画，避免性能问题 |

---

## 🎯 总结

### 核心原则（一句话）

> **避免造轮子，提前分配资源，避免资源泄露，无锁业务层，容易调试，少状态机，简化 UI。**

### 团队定位

| 层级 | 由谁负责 | 技术 | 学习成本 |
|------|---------|------|---------|
| **业务层（你们团队）** | 歌单、榜单、搜索、逻辑 | C++接口 == Java习惯（无锁） | ⭐⭐⭐ 低（2-3周） |
| **服务层（框架）** | 我来给架构/模板 | 事件驱动 + 上下文封装 | ❌ 不接触 |
| **系统层（第三方）** | 开源库 | LVGL / WebSockets / libcurl / cJSON | ❌ 不接触 |

### 开发流程

1. **使用 Cursor AI**：复制提示模板，生成符合规范的代码
2. **检查资源管理**：确保没有 `new/delete`，使用 Singleton
3. **检查线程安全**：确保业务层无锁，跨线程用事件队列
4. **检查 UI 简化**：确保预创建控件，使用 `show()/hide()`
5. **检查状态机**：确保使用简单枚举，避免复杂状态转换
6. **检查调试**：确保使用统一日志，错误信息清晰

---

**最后更新**: 2025-12-30  
**状态**: ✅ 核心文档，Android/Web/Python 背景工程师 C++ 开发指南

