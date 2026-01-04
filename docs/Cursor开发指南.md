# Cursor 开发指南（F133 / Tina Linux）

> **文档版本**：v2.0（合并版）  
> **最后更新**：2025-12-30  
> **状态**：✅ 核心文档（合并版）  
> **用途**：复制给 Cursor，生成符合规范的代码

---

## 📋 目录

1. [核心架构约束](#一-核心架构约束)
2. [代码生成模板](#二-代码生成模板)
3. [实现指导](#三-实现指导)
4. [代码审查提示](#四-代码审查提示)

---

## 一、核心架构约束

### 1.1 完整架构约束（复制给 Cursor）

```
We are implementing a KTVLV project (F133/Tina Linux) with the following architecture:

**Resource Management Principles:**
- ALL resources are Singleton (no new/delete/free)
- UI controls created once, never deleted (use show()/hide())
- Lifecycle = App lifecycle (no manual release)
- No malloc/free/new/delete in business code

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

**Forbidden:**
- ❌ new/delete/free/lv_obj_del in business code
- ❌ Creating controls in loops (use control pool)
- ❌ Creating pages on each navigation (use Singleton + show()/hide())
- ❌ Direct tplayer_* calls (use PlayerService)
- ❌ curl_easy_* in business code (use HttpService)
- ❌ pthread_create in business code (use Service threads)

**Patterns:**
- Singleton pattern for all Services and Pages
- Control pool for lists (pre-create fixed number of items)
- show()/hide() for page navigation
- update() for data refresh (don't recreate controls)
```

### 1.2 并发架构约束（复制给 Cursor）

```
We are implementing a concurrency architecture for an LVGL + tplayer project:

- 4 threads: UI (main), PlayerThread (std::thread), Network (std::async), SDK internal thread.
- 2 message queues only:
  1. PlayerCmdQueue : UI/Network -> PlayerThread (commands)
  2. UiEventQueue   : SDK/PlayerThread/Network -> UI (events)

Rules:
- Command Down, Event Up
- UI never calls tplayer directly
- tplayer callbacks always push events to UiEventQueue, then UiDispatcher::post() to return to UI thread
- std::queue + mutex inside, no lock exposed to business layer
- No moodycamel, no boost, no raw pthread for business
- No direct cross-thread widget updates

Implement:
- PlayerCmdQueue { enqueue/can block, consumed by PlayerThread loop }
- UiEventQueue { push/drain, consumed via UiDispatcher::post }
- PlayerAdapter that translates commands to tplayer_*()
- Translate SDK callbacks to PlayerEvent and push into UiEventQueue

Primary patterns:
- Single Consumer Queue
- Event Driven
- Zero Shared State for business logic
```

---

## 二、代码生成模板

### 2.1 Service 模板

```cpp
// ServiceName.h
#pragma once

class ServiceName {
public:
    static ServiceName& instance();
    
    // 初始化（只调用一次）
    void init();
    
    // 业务接口
    void doSomething();
    
private:
    ServiceName() = default;
    ~ServiceName() = default;
    ServiceName(const ServiceName&) = delete;
    ServiceName& operator=(const ServiceName&) = delete;
    
    bool m_initialized = false;
};

// ServiceName.cpp
ServiceName& ServiceName::instance() {
    static ServiceName inst;
    return inst;
}

void ServiceName::init() {
    if (m_initialized) return;
    // 初始化逻辑（只执行一次）
    m_initialized = true;
}
```

### 2.2 Page 模板

```cpp
// PageName.h
#pragma once
#include "ui/BasePage.h"

class PageName : public BasePage {
public:
    static PageName& instance();
    
    void show() override;
    void hide() override;
    void update(const DataType& data);
    
private:
    PageName();  // 私有构造函数
    
    void buildUI();  // 只调用一次
    
    lv_obj_t* m_label = nullptr;
    // ... 其他控件
};

// PageName.cpp
PageName::PageName() {
    root = lv_obj_create(lv_scr_act());
    buildUI();
    lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
}

PageName& PageName::instance() {
    static PageName inst;
    return inst;
}

void PageName::buildUI() {
    // 创建所有控件（只调用一次）
    m_label = lv_label_create(root);
    // ... 其他控件
}

void PageName::show() {
    lv_obj_clear_flag(root, LV_OBJ_FLAG_HIDDEN);
}

void PageName::hide() {
    lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
}

void PageName::update(const DataType& data) {
    // 更新已有控件，不创建新控件
    lv_label_set_text(m_label, data.text.c_str());
}
```

### 2.3 List View 模板（控件池）

```cpp
// ListView.h
class ListView {
private:
    static constexpr int POOL_SIZE = 50;
    lv_obj_t* m_itemPool[POOL_SIZE] = {nullptr};
    lv_obj_t* m_root = nullptr;
    
public:
    ListView(lv_obj_t* parent);
    
    void updateList(const std::vector<ItemType>& items);
    
private:
    void buildItemPool();
    void setItemContent(lv_obj_t* item, const ItemType& data);
};

// ListView.cpp
ListView::ListView(lv_obj_t* parent) {
    m_root = lv_obj_create(parent);
    buildItemPool();
}

void ListView::buildItemPool() {
    // 预创建固定数量项
    for(int i = 0; i < POOL_SIZE; i++) {
        m_itemPool[i] = lv_list_add_btn(m_root, NULL, "");
        lv_obj_add_flag(m_itemPool[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void ListView::updateList(const std::vector<ItemType>& items) {
    for(int i = 0; i < POOL_SIZE; i++) {
        if(i < items.size()) {
            setItemContent(m_itemPool[i], items[i]);
            lv_obj_clear_flag(m_itemPool[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(m_itemPool[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}
```

### 2.4 Controller 模板

```cpp
// ControllerName.h
#pragma once
#include "services/HttpService.h"
#include "services/PlayerService.h"
#include "services/UiEventBus.h"
#include "models/DataModel.h"

class ControllerName {
public:
    static ControllerName& instance();
    
    void onEvent(const EventType& event);
    void onAction(const ActionType& action);
    
private:
    ControllerName() = default;
    
    void handleData(const DataType& data);
};
```

---

## 三、实现指导

### 3.1 tplayer 的创建/销毁/回调绑定

在 `PlayerAdapter::Impl` 构造函数中：

```cpp
PlayerAdapter::Impl::Impl() {
    // TODO: 初始化 tplayer
    tp_ = TPlayerCreate();
    if (!tp_) {
        // 错误处理
        return;
    }
    
    // TODO: 设置回调（C风格回调，需要桥接到成员函数）
    // TPlayerSetCallback(tp_, tplayer_callback_bridge, this);
}
```

### 3.2 handleCmd 里真正调用 tplayer_*

在 `PlayerAdapter::Impl::handleCmd` 中：

```cpp
switch (cmd.type) {
case PlayerCmdType::PLAY:
    // 停止当前播放
    TPlayerStop(tp_);
    // 设置数据源
    TPlayerSetDataSource(tp_, cmd.url.c_str(), nullptr);
    // 准备播放
    TPlayerPrepare(tp_);
    // 开始播放
    TPlayerStart(tp_);
    break;
case PlayerCmdType::PAUSE:
    TPlayerPause(tp_);
    break;
case PlayerCmdType::RESUME:
    TPlayerStart(tp_);  // 或 TPlayerResume(tp_)
    break;
case PlayerCmdType::REPLAY:
    TPlayerSeekTo(tp_, 0);
    break;
case PlayerCmdType::SWITCH_TRACK:
    TPlayerSwitchAudio(tp_, cmd.value);
    break;
case PlayerCmdType::SET_VOLUME:
    float vol = cmd.value / 100.0f;
    TPlayerSetVolume(tp_, vol, vol);
    break;
case PlayerCmdType::STOP:
    TPlayerStop(tp_);
    break;
case PlayerCmdType::EXIT:
    TPlayerStop(tp_);
    // 不释放tp_，因为可能还会复用
    running_ = false;
    break;
}
```

### 3.3 onSdkEvent 里把 SDK event → PlayerEvent

在 `PlayerAdapter::Impl::onSdkEvent` 中：

```cpp
void PlayerAdapter::Impl::onSdkEvent(int code, int extra) {
    PlayerEvent ev;
    
    // TODO: 根据 tplayer 的 event code 转换
    // 参考 TPlayer 文档中的事件码定义
    switch (code) {
    case TPLAYER_NOTIFY_PREPARED:  // 假设的事件码
        ev.type = PlayerEventType::PREPARING;
        break;
    case TPLAYER_NOTIFY_PLAYBACK_COMPLETE:
        ev.type = PlayerEventType::COMPLETED;
        break;
    case TPLAYER_NOTIFY_ERROR:
        ev.type = PlayerEventType::ERROR;
        ev.error_code = extra;
        break;
    // ... 其他事件
    default:
        return;  // 忽略未知事件
    }
    
    emitToUi(ev);
}
```

### 3.4 C风格回调桥接

因为 tplayer 是 C API，需要桥接到 C++ 成员函数：

```cpp
// player_adapter.cpp 中的静态函数
extern "C" {
    static void tplayer_callback_bridge(void* user_data, int code, int extra) {
        auto impl = static_cast<PlayerAdapter::Impl*>(user_data);
        impl->onSdkEvent(code, extra);
    }
}

// 在 Impl 构造函数中绑定
PlayerAdapter::Impl::Impl() {
    tp_ = TPlayerCreate();
    TPlayerSetCallback(tp_, tplayer_callback_bridge, this);
}
```

### 3.5 UI 使用示例

#### 在页面中使用 PlayerAdapter

```cpp
// 在页面初始化时设置监听器
void PagePlayer::onCreate() {
    PlayerAdapter::instance().setListener([this](const PlayerEvent& ev){
        onPlayerEvent(ev);
    });
}

// 点击播放按钮
void PagePlayer::onPlayButtonClick() {
    std::string url = "http://example.com/song.m3u8";
    PlayerAdapter::instance().play(url);
}

// 点击暂停按钮
void PagePlayer::onPauseButtonClick() {
    PlayerAdapter::instance().pause();
}

// 切换音轨
void PagePlayer::onSwitchTrackButtonClick() {
    int mode = currentTrackMode == 0 ? 1 : 0;
    PlayerAdapter::instance().switchTrack(mode);
}

// 处理播放器事件
void PagePlayer::onPlayerEvent(const PlayerEvent& ev) {
    switch (ev.type) {
    case PlayerEventType::PLAYING:
        updatePlayButton("暂停");
        break;
    case PlayerEventType::PAUSED:
        updatePlayButton("播放");
        break;
    case PlayerEventType::COMPLETED:
        playNextSong();
        break;
    case PlayerEventType::ERROR:
        showError(ev.message);
        break;
    // ...
    }
}
```

---

## 四、代码审查提示

生成代码后，检查以下项：

- [ ] 是否使用 Singleton 模式？
- [ ] 是否有 `new`/`delete`/`free`/`lv_obj_del`？
- [ ] 控件是否在循环内创建？
- [ ] 页面切换是否使用 `show()/hide()`？
- [ ] 是否使用 Service 层接口？
- [ ] 事件是否通过 `UiEventBus`？

### 注意事项

#### 1. 线程安全

- ✅ PlayerAdapter 的所有 public 方法都是线程安全的（内部使用队列）
- ✅ UI 可以在任何时候调用 PlayerAdapter，不需要考虑线程问题
- ❌ 不要在非UI线程直接更新LVGL控件

#### 2. 生命周期

- ✅ 应用启动时调用 `PlayerAdapter::instance().start()`
- ✅ 应用退出时调用 `PlayerAdapter::instance().shutdown()`
- ✅ 确保在 LVGL 初始化之后启动

#### 3. 错误处理

- ✅ tplayer 的错误通过 PlayerEvent 返回
- ✅ UI 层负责显示错误提示
- ❌ 不要在播放器线程中弹窗或更新UI

#### 4. 内存管理

- ✅ 使用 std::string 管理 URL 和消息字符串
- ✅ 队列内部自动管理内存
- ❌ 不需要手动 delete 事件或命令

---

## 🚀 下一步实现

1. ✅ 引入 TPlayer 头文件
2. ✅ 实现 tplayer 的创建/销毁
3. ✅ 实现 tplayer 回调桥接
4. ✅ 在 handleCmd 中调用 tplayer_* API
5. ✅ 在 onSdkEvent 中转换 tplayer 事件码

---

## 📚 相关文档

- **资源管理规范**：[资源管理规范v1.md](./资源管理规范v1.md)
- **团队开发规范**：[团队开发规范v1.md](./团队开发规范v1.md)
- **服务层API设计**：[服务层API设计文档.md](./服务层API设计文档.md)
- **并发架构总结构（最终版）**: [并发架构总结构（最终版）.md](./architecture/并发架构总结构（最终版）.md)
- **事件架构规范**: [事件架构规范.md](./architecture/事件架构规范.md)
- **线程与消息队列架构设计总稿**: [线程与消息队列架构设计总稿.md](./architecture/线程与消息队列架构设计总稿.md)

---

**最后更新**: 2025-12-30  
**状态**: ✅ 核心文档（合并版，包含开发脚手架提示和实现指导）


