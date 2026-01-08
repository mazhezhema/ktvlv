#include "history_db_service.h"
#include "utils/sqlite_helper.h"
#include "utils/log_macros.h"
#include <ctime>
#include <cstdlib>

using ktv::utils::SqliteHelper;
using ktv::utils::SqlRow;

namespace ktv::services {

HistoryDbService::~HistoryDbService() {
    shutdown();
}

int HistoryDbService::initialize(const std::string& db_path, int max_count) {
    if (initialized_) {
        KTV_LOG_WARN("db", "action=init reason=already_initialized");
        return -1;
    }
    
    max_count_ = max_count;
    
    // 初始化 SqliteHelper（进程唯一 DB）
    if (SqliteHelper::Init(db_path.c_str()) != 0) {
        KTV_LOG_ERR("db", "action=init reason=sqlite_init_failed path=%s", db_path.c_str());
        return -1;
    }
    
    // 建表
    const char* create_table_sql =
        "CREATE TABLE IF NOT EXISTS history ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "song_id TEXT NOT NULL,"
        "song_name TEXT,"
        "artist TEXT,"
        "local_path TEXT,"
        "played_at INTEGER NOT NULL"
        ");";
    
    if (SqliteHelper::Exec(create_table_sql) != 0) {
        KTV_LOG_ERR("db", "action=create_table reason=failed");
        return -1;
    }
    
    initialized_ = true;
    KTV_LOG_INFO("db", "action=init path=%s max_count=%d", db_path.c_str(), max_count);
    
    return 0;
}

void HistoryDbService::shutdown() {
    if (!initialized_) {
        return;
    }
    
    SqliteHelper::Shutdown();
    initialized_ = false;
    KTV_LOG_INFO("db", "action=shutdown");
}

std::string HistoryDbService::escapeSql(const std::string& str) {
    std::string result;
    result.reserve(str.size() * 2);
    for (char c : str) {
        if (c == '\'') {
            result += "''";
        } else {
            result += c;
        }
    }
    return result;
}

int HistoryDbService::addRecord(const std::string& song_id,
                                const std::string& song_name,
                                const std::string& artist,
                                const std::string& local_path) {
    if (!initialized_) {
        KTV_LOG_ERR("db", "action=add_record reason=not_initialized");
        return -1;
    }
    
    int64_t now = static_cast<int64_t>(std::time(nullptr));
    
    std::string sql =
        "INSERT INTO history (song_id, song_name, artist, local_path, played_at) VALUES ('" +
        escapeSql(song_id) + "','" +
        escapeSql(song_name) + "','" +
        escapeSql(artist) + "','" +
        escapeSql(local_path) + "'," +
        std::to_string(now) + ");";
    
    if (SqliteHelper::Exec(sql.c_str()) != 0) {
        KTV_LOG_ERR("db", "action=insert song_id=%s", song_id.c_str());
        return -1;
    }
    
    // 裁剪到 max_count
    int trim_ret = trimToMaxCount();
    if (trim_ret != 0) {
        KTV_LOG_WARN("db", "action=trim reason=failed");
        // 不返回失败，因为插入已成功
    }
    
    KTV_LOG_DEBUG("db", "action=add_record song_id=%s", song_id.c_str());
    return 0;
}

int HistoryDbService::getHistoryList(std::vector<HistoryDbItem>& out_items, int max_count) const {
    out_items.clear();
    
    if (!initialized_) {
        KTV_LOG_ERR("db", "action=get_list reason=not_initialized");
        return -1;
    }
    
    std::string sql =
        "SELECT id, song_id, song_name, artist, local_path, played_at "
        "FROM history ORDER BY played_at DESC LIMIT " + std::to_string(max_count) + ";";
    
    std::vector<SqlRow> rows;
    if (SqliteHelper::Query(sql.c_str(), rows) != 0) {
        KTV_LOG_ERR("db", "action=query reason=failed");
        return -1;
    }
    
    for (const auto& row : rows) {
        if (row.cols.size() < 6) continue;
        
        HistoryDbItem item;
        item.id = std::atoll(row.cols[0].c_str());
        item.song_id = row.cols[1];
        item.song_name = row.cols[2];
        item.artist = row.cols[3];
        item.local_path = row.cols[4];
        item.played_at = std::atoll(row.cols[5].c_str());
        
        out_items.push_back(std::move(item));
    }
    
    return 0;
}

int HistoryDbService::clear() {
    if (!initialized_) {
        KTV_LOG_ERR("db", "action=clear reason=not_initialized");
        return -1;
    }
    
    if (SqliteHelper::Exec("DELETE FROM history;") != 0) {
        KTV_LOG_ERR("db", "action=clear reason=failed");
        return -1;
    }
    
    KTV_LOG_INFO("db", "action=clear");
    return 0;
}

int HistoryDbService::getCount(int& out_count) const {
    out_count = 0;
    
    if (!initialized_) {
        return -1;
    }
    
    std::vector<SqlRow> rows;
    if (SqliteHelper::Query("SELECT COUNT(*) FROM history;", rows) != 0) {
        return -1;
    }
    
    if (!rows.empty() && !rows[0].cols.empty()) {
        out_count = std::atoi(rows[0].cols[0].c_str());
    }
    
    return 0;
}

int HistoryDbService::trimToMaxCount() {
    if (!initialized_) {
        return -1;
    }
    
    std::string sql =
        "DELETE FROM history WHERE id NOT IN "
        "(SELECT id FROM history ORDER BY played_at DESC LIMIT " +
        std::to_string(max_count_) + ");";
    
    return SqliteHelper::Exec(sql.c_str());
}

}  // namespace ktv::services
