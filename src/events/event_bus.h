#ifndef KTVLV_EVENTS_EVENT_BUS_H
#define KTVLV_EVENTS_EVENT_BUS_H

#include "event_types.h"
#include <queue>
#include <mutex>
#include <condition_variable>

namespace ktv::events {

class EventBus {
public:
    static EventBus& getInstance() {
        static EventBus instance;
        return instance;
    }

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    void publish(const Event& ev);
    bool poll(Event& ev);

    /**
     * 在主线程中分发事件（从队列中取出所有事件并处理）
     * 此方法必须在调用 lv_timer_handler() 的线程中执行
     * 确保所有 UI 更新都在主线程进行，避免多线程访问 LVGL 导致崩溃
     */
    void dispatchOnUiThread();

private:
    EventBus() = default;
    ~EventBus() = default;

    std::queue<Event> queue_;
    std::mutex mutex_;
    std::condition_variable condition_;
};

}  // namespace ktv::events

#endif  // KTVLV_EVENTS_EVENT_BUS_H

