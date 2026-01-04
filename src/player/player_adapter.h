// player_adapter.h
#pragma once
#include "player_event.h"
#include <functional>
#include <string>

using PlayerListener = std::function<void(const PlayerEvent&)>;

class PlayerAdapter {
public:
    static PlayerAdapter& instance();

    // 业务 / UI 层 API（线程安全，内部入队）
    void play(const std::string& url);    // 点歌 / 切歌
    void pause();
    void resume();
    void replay();                        // 重唱
    void switchTrack(int mode);           // 原/伴奏切换
    void setVolume(int volume);           // 0-100
    void stop();                          // 停止当前
    void exit();                          // 退出播放器

    void setListener(PlayerListener listener);

    // 生命周期
    void start(); // 启动内部播放器线程
    void shutdown(); // 停止线程，释放资源

private:
    PlayerAdapter();
    ~PlayerAdapter();
    PlayerAdapter(const PlayerAdapter&) = delete;
    PlayerAdapter& operator=(const PlayerAdapter&) = delete;

    class Impl;
    Impl* impl_; // PIMPL 隐藏实现细节
};



