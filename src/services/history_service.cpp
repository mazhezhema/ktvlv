#include "history_service.h"
#include <plog/Log.h>

namespace ktv::services {

void HistoryService::add(const HistoryItem& item) {
    if (items_.size() >= capacity_) {
        items_.pop_front();
    }
    items_.push_back(item);
    PLOGD << "History add: " << item.title << " / " << item.artist;
}

}  // namespace ktv::services

