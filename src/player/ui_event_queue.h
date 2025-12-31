// ui_event_queue.h
#pragma once
#include <queue>
#include <mutex>
#include <functional>

template<typename EventT>
class UiEventQueue {
public:
    // 任意后台线程调用：丢事件
    void push(EventT ev) {
        std::lock_guard<std::mutex> lock(mtx_);
        queue_.push(std::move(ev));
    }

    // UI线程调用：一次性消费所有事件
    void drain(const std::function<void(const EventT&)>& handler) {
        std::queue<EventT> tmp;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            std::swap(tmp, queue_);
        }
        while (!tmp.empty()) {
            handler(tmp.front());
            tmp.pop();
        }
    }

private:
    std::queue<EventT> queue_;
    std::mutex mtx_;
};

