#include "history_service.h"
#include <syslog.h>

namespace ktv::services {

void HistoryService::add(const HistoryItem& item) {
    if (items_.size() >= capacity_) {
        items_.pop_front();
    }
    items_.push_back(item);
    syslog(LOG_DEBUG, "[ktv][history][action] action=add title=%s artist=%s", item.title.c_str(), item.artist.c_str());
}

}  // namespace ktv::services

