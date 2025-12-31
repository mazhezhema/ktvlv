// player_cmd_queue.cpp
#include "player_cmd_queue.h"

void PlayerCmdQueue::enqueue(const PlayerCmd& cmd) {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (stopped_) return;
        queue_.push(cmd);
    }
    cv_.notify_one();
}

PlayerCmd PlayerCmdQueue::waitDequeue() {
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, [this]{
        return stopped_ || !queue_.empty();
    });

    if (stopped_ && queue_.empty()) {
        // 返回一个特殊命令，调用方可以自行判断
        return PlayerCmd{PlayerCmdType::EXIT, "", 0};
    }

    PlayerCmd cmd = queue_.front();
    queue_.pop();
    return cmd;
}

std::optional<PlayerCmd> PlayerCmdQueue::tryDequeue() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (queue_.empty()) return std::nullopt;
    PlayerCmd cmd = queue_.front();
    queue_.pop();
    return cmd;
}

void PlayerCmdQueue::stop() {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        stopped_ = true;
    }
    cv_.notify_all();
}

