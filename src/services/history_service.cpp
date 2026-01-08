#include "history_service.h"
#include "history_db_service.h"
#include "utils/log_macros.h"

namespace ktv::services {

HistoryService::~HistoryService() {
    shutdown();
}

int HistoryService::initialize(const std::string& db_path, int max_count) {
    if (initialized_) {
        KTV_LOG_WARN("history", "action=init reason=already_initialized");
        return -1;
    }
    
    max_count_ = max_count;
    
    int ret = HistoryDbService::instance().initialize(db_path, max_count);
    if (ret != 0) {
        KTV_LOG_ERR("history", "action=init reason=db_failed");
        return ret;
    }
    
    initialized_ = true;
    KTV_LOG_INFO("history", "action=init path=%s max_count=%d", db_path.c_str(), max_count);
    
    return 0;
}

void HistoryService::shutdown() {
    if (!initialized_) {
        return;
    }
    
    HistoryDbService::instance().shutdown();
    initialized_ = false;
    KTV_LOG_INFO("history", "action=shutdown");
}

void HistoryService::setCapacity(size_t cap) {
    max_count_ = static_cast<int>(cap);
    KTV_LOG_INFO("history", "action=set_capacity max_count=%zu", cap);
}

int HistoryService::add(const HistoryItem& item) {
    if (!initialized_) {
        KTV_LOG_ERR("history", "action=add reason=not_initialized");
        return -1;
    }
    
    // 使用 song_id（如果提供），否则使用 title 作为 song_id
    std::string song_id = item.song_id.empty() ? item.title : item.song_id;
    
    int ret = HistoryDbService::instance().addRecord(
        song_id,
        item.title,
        item.artist,
        item.local_path
    );
    
    if (ret != 0) {
        KTV_LOG_ERR("history", "action=add song_id=%s", song_id.c_str());
        return ret;
    }
    
    KTV_LOG_DEBUG("history", "action=add title=%s artist=%s", item.title.c_str(), item.artist.c_str());
    return 0;
}

int HistoryService::getItems(std::vector<HistoryItem>& out_items) const {
    out_items.clear();
    
    if (!initialized_) {
        KTV_LOG_ERR("history", "action=get_items reason=not_initialized");
        return -1;
    }
    
    std::vector<HistoryDbItem> db_items;
    int ret = HistoryDbService::instance().getHistoryList(db_items, max_count_);
    if (ret != 0) {
        return ret;
    }
    
    out_items.reserve(db_items.size());
    for (const auto& db_item : db_items) {
        HistoryItem item;
        item.song_id = db_item.song_id;
        item.title = db_item.song_name;
        item.artist = db_item.artist;
        item.local_path = db_item.local_path;
        out_items.push_back(std::move(item));
    }
    
    return 0;
}

int HistoryService::clear() {
    if (!initialized_) {
        KTV_LOG_ERR("history", "action=clear reason=not_initialized");
        return -1;
    }
    
    return HistoryDbService::instance().clear();
}

int HistoryService::getCount(int& out_count) const {
    out_count = 0;
    
    if (!initialized_) {
        return -1;
    }
    
    return HistoryDbService::instance().getCount(out_count);
}

}  // namespace ktv::services
