// player_adapter.cpp
#include "player_adapter.h"
#include "player_cmd_queue.h"
#include "ui_event_queue.h"
#include "ui_dispatcher.h"

// TODO: 引入 tplayer 头文件
// #include "tplayer.h"

#include <thread>
#include <atomic>
#include <iostream> // 临时调试用

// -------------------- PlayerAdapter::Impl --------------------

class PlayerAdapter::Impl {
public:
    Impl();
    ~Impl();

    void start();
    void shutdown();

    void play(const std::string& url);
    void pause();
    void resume();
    void replay();
    void switchTrack(int mode);
    void setVolume(int volume);
    void stop();
    void exit();

    void setListener(PlayerListener listener);

    // SDK 回调入口（由 C 回调桥接调用）
    void onSdkEvent(int code, int extra);

private:
    void threadLoop();               // 播放器线程主循环
    void handleCmd(const PlayerCmd& cmd);
    void emitToUi(const PlayerEvent& ev);

    std::thread worker_;
    std::atomic<bool> running_{false};

    PlayerCmdQueue cmdQueue_;
    UiEventQueue<PlayerEvent> uiQueue_;

    // tplayer 相关
    // TPLAYER* tp_ = nullptr; // TODO: 替换为真实类型

    PlayerListener listener_; // 仅在UI线程访问
};

// -------------------- PlayerAdapter::Impl实现 --------------------

PlayerAdapter::Impl::Impl() {
    // TODO: 初始化 tplayer
    // tp_ = tplayer_create();
    // tplayer_set_callback(tp_, c_style_callback, this);
}

PlayerAdapter::Impl::~Impl() {
    shutdown();
    // TODO: 释放 tplayer
    // if (tp_) {
    //     tplayer_destroy(tp_);
    //     tp_ = nullptr;
    // }
}

void PlayerAdapter::Impl::start() {
    if (running_.exchange(true)) return;
    worker_ = std::thread([this]{
        threadLoop();
    });
}

void PlayerAdapter::Impl::shutdown() {
    if (!running_.exchange(false)) return;
    cmdQueue_.stop();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void PlayerAdapter::Impl::play(const std::string& url) {
    PlayerCmd cmd{PlayerCmdType::PLAY, url, 0};
    cmdQueue_.enqueue(cmd);
}

void PlayerAdapter::Impl::pause() {
    cmdQueue_.enqueue(PlayerCmd{PlayerCmdType::PAUSE, "", 0});
}

void PlayerAdapter::Impl::resume() {
    cmdQueue_.enqueue(PlayerCmd{PlayerCmdType::RESUME, "", 0});
}

void PlayerAdapter::Impl::replay() {
    cmdQueue_.enqueue(PlayerCmd{PlayerCmdType::REPLAY, "", 0});
}

void PlayerAdapter::Impl::switchTrack(int mode) {
    PlayerCmd cmd{PlayerCmdType::SWITCH_TRACK, "", mode};
    cmdQueue_.enqueue(cmd);
}

void PlayerAdapter::Impl::setVolume(int volume) {
    PlayerCmd cmd{PlayerCmdType::SET_VOLUME, "", volume};
    cmdQueue_.enqueue(cmd);
}

void PlayerAdapter::Impl::stop() {
    cmdQueue_.enqueue(PlayerCmd{PlayerCmdType::STOP, "", 0});
}

void PlayerAdapter::Impl::exit() {
    cmdQueue_.enqueue(PlayerCmd{PlayerCmdType::EXIT, "", 0});
}

void PlayerAdapter::Impl::setListener(PlayerListener listener) {
    // 只在UI线程调用
    listener_ = std::move(listener);
}

void PlayerAdapter::Impl::threadLoop() {
    while (running_) {
        PlayerCmd cmd = cmdQueue_.waitDequeue();
        handleCmd(cmd);
    }
}

void PlayerAdapter::Impl::handleCmd(const PlayerCmd& cmd) {
    // TODO: 根据 cmd.type 调用 tplayer_* 接口
    // + 做好状态机保护（PREPARING/PLAYING/PAUSED/...）

    switch (cmd.type) {
    case PlayerCmdType::PLAY:
        // tplayer_stop(tp_);
        // tplayer_set_data_source(tp_, cmd.url.c_str());
        // tplayer_prepare_async(tp_);
        std::cout << "[PlayerAdapter] PLAY: " << cmd.url << std::endl;
        break;
    case PlayerCmdType::PAUSE:
        // tplayer_pause(tp_);
        std::cout << "[PlayerAdapter] PAUSE" << std::endl;
        break;
    case PlayerCmdType::RESUME:
        // tplayer_resume(tp_);
        std::cout << "[PlayerAdapter] RESUME" << std::endl;
        break;
    case PlayerCmdType::REPLAY:
        // tplayer_seek(tp_, 0);
        std::cout << "[PlayerAdapter] REPLAY" << std::endl;
        break;
    case PlayerCmdType::SWITCH_TRACK:
        // tplayer_set_track_mode(tp_, cmd.value);
        std::cout << "[PlayerAdapter] SWITCH_TRACK: " << cmd.value << std::endl;
        break;
    case PlayerCmdType::SET_VOLUME:
        // tplayer_set_volume(tp_, cmd.value);
        std::cout << "[PlayerAdapter] SET_VOLUME: " << cmd.value << std::endl;
        break;
    case PlayerCmdType::STOP:
        // tplayer_stop(tp_);
        std::cout << "[PlayerAdapter] STOP" << std::endl;
        break;
    case PlayerCmdType::EXIT:
        // tplayer_stop(tp_);
        // tplayer_reset(tp_);
        std::cout << "[PlayerAdapter] EXIT" << std::endl;
        running_ = false;
        break;
    }
}

void PlayerAdapter::Impl::emitToUi(const PlayerEvent& ev) {
    uiQueue_.push(ev);

    UiDispatcher::post([this](){
        uiQueue_.drain([this](const PlayerEvent& ev){
            if (listener_) {
                listener_(ev);
            }
        });
    });
}

void PlayerAdapter::Impl::onSdkEvent(int code, int extra) {
    // TODO: 把 tplayer 的 event code 转成 PlayerEvent
    PlayerEvent ev;
    // switch(code) { ... 填充 ev ... }
    emitToUi(ev);
}

// -------------------- PlayerAdapter 外层包装 --------------------

PlayerAdapter& PlayerAdapter::instance() {
    static PlayerAdapter inst;
    return inst;
}

PlayerAdapter::PlayerAdapter()
    : impl_(new Impl()) {
}

PlayerAdapter::~PlayerAdapter() {
    delete impl_;
}

void PlayerAdapter::start() {
    impl_->start();
}

void PlayerAdapter::shutdown() {
    impl_->shutdown();
}

void PlayerAdapter::play(const std::string& url)      { impl_->play(url); }
void PlayerAdapter::pause()                           { impl_->pause(); }
void PlayerAdapter::resume()                          { impl_->resume(); }
void PlayerAdapter::replay()                          { impl_->replay(); }
void PlayerAdapter::switchTrack(int mode)             { impl_->switchTrack(mode); }
void PlayerAdapter::setVolume(int volume)             { impl_->setVolume(volume); }
void PlayerAdapter::stop()                            { impl_->stop(); }
void PlayerAdapter::exit()                            { impl_->exit(); }
void PlayerAdapter::setListener(PlayerListener l)     { impl_->setListener(std::move(l)); }




