// player_cmd_queue.h
#pragma once
#include "player_cmd.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

class PlayerCmdQueue {
public:
    // 生产者线程调用：推入一个命令（非阻塞）
    void enqueue(const PlayerCmd& cmd);

    // 播放器线程调用：阻塞等待直到有命令
    PlayerCmd waitDequeue();

    // 播放器线程调用：尝试取一个命令（无则返回nullopt）
    std::optional<PlayerCmd> tryDequeue();

    // 停止用：唤醒等待线程（例如退出时）
    void stop();

private:
    std::queue<PlayerCmd> queue_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stopped_ = false;
};

