# Cursor 实现提示模板

> **文档版本**：v1.0  
> **最后更新**：2025-12-30  
> **用途**：直接复制给 Cursor 使用

---

## 📋 架构约束（复制给 Cursor）

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

## 🔧 实现指导（填 TODO 的方向）

### 1. tplayer 的创建/销毁/回调绑定

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

### 2. handleCmd 里真正调用 tplayer_*

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

### 3. onSdkEvent 里把 SDK event → PlayerEvent

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

### 4. C风格回调桥接

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

---

## 🎨 UI 使用示例

### 在页面中使用 PlayerAdapter

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

## 📝 注意事项

### 1. 线程安全

- ✅ PlayerAdapter 的所有 public 方法都是线程安全的（内部使用队列）
- ✅ UI 可以在任何时候调用 PlayerAdapter，不需要考虑线程问题
- ❌ 不要在非UI线程直接更新LVGL控件

### 2. 生命周期

- ✅ 应用启动时调用 `PlayerAdapter::instance().start()`
- ✅ 应用退出时调用 `PlayerAdapter::instance().shutdown()`
- ✅ 确保在 LVGL 初始化之后启动

### 3. 错误处理

- ✅ tplayer 的错误通过 PlayerEvent 返回
- ✅ UI 层负责显示错误提示
- ❌ 不要在播放器线程中弹窗或更新UI

### 4. 内存管理

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

- **并发架构总结构（最终版）**: [并发架构总结构（最终版）.md](./architecture/并发架构总结构（最终版）.md)
- **事件架构规范**: [事件架构规范.md](./architecture/事件架构规范.md)
- **线程与消息队列架构设计总稿**: [线程与消息队列架构设计总稿.md](./architecture/线程与消息队列架构设计总稿.md)

---

**最后更新**: 2025-12-30


