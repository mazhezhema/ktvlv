#include "history_service.h"
#include "history_db_service.h"
#include <syslog.h>

namespace ktv::services {

HistoryService::~HistoryService() {
    shutdown();
}

bool HistoryService::initialize(const std::string& db_path, int max_count) {
    if (initialized_) {
        syslog(LOG_WARNING, "[ktv][history][error] component=history_service reason=already_initialized");
        return false;
    }
    
    max_count_ = max_count;
    
    if (!HistoryDbService::instance().initialize(db_path, max_count)) {
        syslog(LOG_ERR, "[ktv][history][error] component=history_service action=init_db_failed");
        return false;
    }
    
    initialized_ = true;
    syslog(LOG_INFO, "[ktv][history][init] component=history_service db_path=%s max_count=%d", db_path.c_str(), max_count);
    
    return true;
}

void HistoryService::shutdown() {
    if (!initialized_) {
        return;
    }
    
    HistoryDbService::instance().shutdown();
    initialized_ = false;
    syslog(LOG_INFO, "[ktv][history][shutdown] component=history_service");
}

void HistoryService::setCapacity(size_t cap) {
    max_count_ = static_cast<int>(cap);
    // 注意：HistoryDbService 的 max_count 在初始化时设置，这里只更新本地变量
    // 如果需要动态修改，需要重新初始化 HistoryDbService
    syslog(LOG_INFO, "[ktv][history][action] component=history_service action=set_capacity max_count=%zu", cap);
}

void HistoryService::add(const HistoryItem& item) {
    if (!initialized_) {
        syslog(LOG_ERR, "[ktv][history][error] component=history_service action=add reason=not_initialized");
        return;
    }
    
    // 使用 song_id（如果提供），否则使用 title 作为 song_id
    std::string song_id = item.song_id.empty() ? item.title : item.song_id;
    
    if (!HistoryDbService::instance().addRecord(
            song_id,
            item.title,
            item.artist,
            item.local_path)) {
        syslog(LOG_ERR, "[ktv][history][error] component=history_service action=add song_id=%s", song_id.c_str());
        return;
    }
    
    syslog(LOG_DEBUG, "[ktv][history][action] component=history_service action=add title=%s artist=%s", 
           item.title.c_str(), item.artist.c_str());
}

std::vector<HistoryItem> HistoryService::items() const {
    std::vector<HistoryItem> result;
    
    if (!initialized_) {
        syslog(LOG_ERR, "[ktv][history][error] component=history_service action=get_items reason=not_initialized");
        return result;
    }
    
    auto db_items = HistoryDbService::instance().getHistoryList(max_count_);
    result.reserve(db_items.size());
    
    for (const auto& db_item : db_items) {
        HistoryItem item;
        item.song_id = db_item.song_id;
        item.title = db_item.song_name;
        item.artist = db_item.artist;
        item.local_path = db_item.local_path;
        result.push_back(std::move(item));
    }
    
    return result;
}

bool HistoryService::clear() {
    if (!initialized_) {
        syslog(LOG_ERR, "[ktv][history][error] component=history_service action=clear reason=not_initialized");
        return false;
    }
    
    return HistoryDbService::instance().clear();
}

int HistoryService::getCount() const {
    if (!initialized_) {
        return 0;
    }
    
    return HistoryDbService::instance().getCount();
}

}  // namespace ktv::services

