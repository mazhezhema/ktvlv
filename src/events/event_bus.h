#ifndef KTVLV_EVENTS_EVENT_BUS_H
#define KTVLV_EVENTS_EVENT_BUS_H

#include "event_types.h"
#include "concurrentqueue.h"
#include <atomic>

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

private:
    EventBus() = default;
    ~EventBus() = default;

    moodycamel::ConcurrentQueue<Event> queue_;
};

}  // namespace ktv::events

#endif  // KTVLV_EVENTS_EVENT_BUS_H

