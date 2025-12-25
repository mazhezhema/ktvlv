#include "event_bus.h"

namespace ktv::events {

void EventBus::publish(const Event& ev) {
    queue_.enqueue(ev);
}

bool EventBus::poll(Event& ev) {
    return queue_.try_dequeue(ev);
}

}  // namespace ktv::events

