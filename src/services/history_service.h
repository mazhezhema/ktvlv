#ifndef KTVLV_SERVICES_HISTORY_SERVICE_H
#define KTVLV_SERVICES_HISTORY_SERVICE_H

#include <deque>
#include <string>

namespace ktv::services {

struct HistoryItem {
    std::string title;
    std::string artist;
    std::string local_path;
};

class HistoryService {
public:
    static HistoryService& getInstance() {
        static HistoryService instance;
        return instance;
    }
    HistoryService(const HistoryService&) = delete;
    HistoryService& operator=(const HistoryService&) = delete;

    void setCapacity(size_t cap) { capacity_ = cap; }
    void add(const HistoryItem& item);
    const std::deque<HistoryItem>& items() const { return items_; }

private:
    HistoryService() = default;
    ~HistoryService() = default;

    size_t capacity_{50};
    std::deque<HistoryItem> items_;
};

}  // namespace ktv::services

#endif  // KTVLV_SERVICES_HISTORY_SERVICE_H

