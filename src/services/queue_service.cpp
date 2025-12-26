#include "queue_service.h"
#include <plog/Log.h>
#include <algorithm>

namespace ktv::services {

void QueueService::add(const QueueItem& item) {
    queue_.push_back(item);
    PLOGI << "Queue add: " << item.title << " / " << item.artist;
}

void QueueService::setCurrentIndex(int index) {
    if (index >= 0 && index < static_cast<int>(queue_.size())) {
        current_index_ = index;
        PLOGI << "Queue current index: " << index;
    } else {
        current_index_ = -1;
    }
}

QueueItem* QueueService::getNext() {
    if (queue_.empty()) {
        return nullptr;
    }
    
    if (current_index_ < 0) {
        current_index_ = 0;
    } else {
        current_index_++;
        if (current_index_ >= static_cast<int>(queue_.size())) {
            current_index_ = 0;  // 循环播放
        }
    }
    
    return &queue_[current_index_];
}

QueueItem* QueueService::getCurrent() {
    if (current_index_ >= 0 && current_index_ < static_cast<int>(queue_.size())) {
        return &queue_[current_index_];
    }
    return nullptr;
}

void QueueService::remove(int index) {
    if (index >= 0 && index < static_cast<int>(queue_.size())) {
        queue_.erase(queue_.begin() + index);
        
        // 调整当前索引
        if (current_index_ == index) {
            current_index_ = -1;
        } else if (current_index_ > index) {
            current_index_--;
        }
        
        PLOGI << "Queue remove index: " << index;
    }
}

void QueueService::clear() {
    queue_.clear();
    current_index_ = -1;
    PLOGI << "Queue cleared";
}

}  // namespace ktv::services


