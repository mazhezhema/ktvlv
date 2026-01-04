#include "history_db_service.h"
#include <syslog.h>
#include <ctime>
#include <cstring>

// SQLite3 头文件（仅在实现文件中包含，不暴露到业务层）
#include <sqlite3.h>

namespace ktv::services {

HistoryDbService::~HistoryDbService() {
    shutdown();
}

bool HistoryDbService::initialize(const std::string& db_path, int max_count) {
    if (initialized_) {
        syslog(LOG_WARNING, "[ktv][db][error] component=history_db reason=already_initialized");
        return false;
    }
    
    db_path_ = db_path;
    max_count_ = max_count;
    
    sqlite3* db = nullptr;
    int ret = sqlite3_open(db_path.c_str(), &db);
    if (ret != SQLITE_OK) {
        syslog(LOG_ERR, "[ktv][db][error] component=history_db action=open path=%s err=%s", 
               db_path.c_str(), db ? sqlite3_errmsg(db) : "unknown");
        if (db) {
            sqlite3_close(db);
        }
        return false;
    }
    
    db_ = static_cast<void*>(db);
    
    // 配置 PRAGMA（轻量级配置）
    execSql("PRAGMA journal_mode=WAL;");
    execSql("PRAGMA synchronous=NORMAL;");
    execSql("PRAGMA temp_store=MEMORY;");
    execSql("PRAGMA cache_size=-512;");  // 512KB
    
    // 创建表
    const char* create_table_sql = R"(
        CREATE TABLE IF NOT EXISTS history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            song_id TEXT NOT NULL,
            song_name TEXT,
            artist TEXT,
            local_path TEXT,
            played_at INTEGER NOT NULL
        );
    )";
    
    if (!execSql(create_table_sql)) {
        syslog(LOG_ERR, "[ktv][db][error] component=history_db action=create_table");
        sqlite3_close(db);
        db_ = nullptr;
        return false;
    }
    
    // 准备 prepared statements
    sqlite3_stmt* stmt = nullptr;
    
    // INSERT 语句
    const char* insert_sql = "INSERT INTO history (song_id, song_name, artist, local_path, played_at) VALUES (?, ?, ?, ?, ?);";
    ret = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, nullptr);
    if (ret != SQLITE_OK) {
        syslog(LOG_ERR, "[ktv][db][error] component=history_db action=prepare_insert err=%s", sqlite3_errmsg(db));
        sqlite3_close(db);
        db_ = nullptr;
        return false;
    }
    stmt_insert_ = static_cast<void*>(stmt);
    
    // SELECT 语句（按时间倒序）
    const char* select_sql = "SELECT id, song_id, song_name, artist, local_path, played_at FROM history ORDER BY played_at DESC LIMIT ?;";
    ret = sqlite3_prepare_v2(db, select_sql, -1, &stmt, nullptr);
    if (ret != SQLITE_OK) {
        syslog(LOG_ERR, "[ktv][db][error] component=history_db action=prepare_select err=%s", sqlite3_errmsg(db));
        sqlite3_finalize(static_cast<sqlite3_stmt*>(stmt_insert_));
        sqlite3_close(db);
        db_ = nullptr;
        return false;
    }
    stmt_select_ = static_cast<void*>(stmt);
    
    // DELETE 语句（裁剪到 max_count）
    const char* delete_sql = "DELETE FROM history WHERE id NOT IN (SELECT id FROM history ORDER BY played_at DESC LIMIT ?);";
    ret = sqlite3_prepare_v2(db, delete_sql, -1, &stmt, nullptr);
    if (ret != SQLITE_OK) {
        syslog(LOG_ERR, "[ktv][db][error] component=history_db action=prepare_delete err=%s", sqlite3_errmsg(db));
        sqlite3_finalize(static_cast<sqlite3_stmt*>(stmt_insert_));
        sqlite3_finalize(static_cast<sqlite3_stmt*>(stmt_select_));
        sqlite3_close(db);
        db_ = nullptr;
        return false;
    }
    stmt_delete_ = static_cast<void*>(stmt);
    
    // COUNT 语句
    const char* count_sql = "SELECT COUNT(*) FROM history;";
    ret = sqlite3_prepare_v2(db, count_sql, -1, &stmt, nullptr);
    if (ret != SQLITE_OK) {
        syslog(LOG_ERR, "[ktv][db][error] component=history_db action=prepare_count err=%s", sqlite3_errmsg(db));
        sqlite3_finalize(static_cast<sqlite3_stmt*>(stmt_insert_));
        sqlite3_finalize(static_cast<sqlite3_stmt*>(stmt_select_));
        sqlite3_finalize(static_cast<sqlite3_stmt*>(stmt_delete_));
        sqlite3_close(db);
        db_ = nullptr;
        return false;
    }
    stmt_count_ = static_cast<void*>(stmt);
    
    // CLEAR 语句
    const char* clear_sql = "DELETE FROM history;";
    ret = sqlite3_prepare_v2(db, clear_sql, -1, &stmt, nullptr);
    if (ret != SQLITE_OK) {
        syslog(LOG_ERR, "[ktv][db][error] component=history_db action=prepare_clear err=%s", sqlite3_errmsg(db));
        sqlite3_finalize(static_cast<sqlite3_stmt*>(stmt_insert_));
        sqlite3_finalize(static_cast<sqlite3_stmt*>(stmt_select_));
        sqlite3_finalize(static_cast<sqlite3_stmt*>(stmt_delete_));
        sqlite3_finalize(static_cast<sqlite3_stmt*>(stmt_count_));
        sqlite3_close(db);
        db_ = nullptr;
        return false;
    }
    stmt_clear_ = static_cast<void*>(stmt);
    
    initialized_ = true;
    syslog(LOG_INFO, "[ktv][db][init] component=history_db path=%s max_count=%d", db_path.c_str(), max_count);
    
    return true;
}

void HistoryDbService::shutdown() {
    if (!initialized_) {
        return;
    }
    
    // 释放 prepared statements
    if (stmt_insert_) {
        sqlite3_finalize(static_cast<sqlite3_stmt*>(stmt_insert_));
        stmt_insert_ = nullptr;
    }
    if (stmt_select_) {
        sqlite3_finalize(static_cast<sqlite3_stmt*>(stmt_select_));
        stmt_select_ = nullptr;
    }
    if (stmt_delete_) {
        sqlite3_finalize(static_cast<sqlite3_stmt*>(stmt_delete_));
        stmt_delete_ = nullptr;
    }
    if (stmt_count_) {
        sqlite3_finalize(static_cast<sqlite3_stmt*>(stmt_count_));
        stmt_count_ = nullptr;
    }
    if (stmt_clear_) {
        sqlite3_finalize(static_cast<sqlite3_stmt*>(stmt_clear_));
        stmt_clear_ = nullptr;
    }
    
    // 关闭数据库
    if (db_) {
        sqlite3_close(static_cast<sqlite3*>(db_));
        db_ = nullptr;
    }
    
    initialized_ = false;
    syslog(LOG_INFO, "[ktv][db][shutdown] component=history_db");
}

bool HistoryDbService::addRecord(const std::string& song_id,
                                 const std::string& song_name,
                                 const std::string& artist,
                                 const std::string& local_path) {
    if (!initialized_) {
        syslog(LOG_ERR, "[ktv][db][error] component=history_db action=add_record reason=not_initialized");
        return false;
    }
    
    sqlite3_stmt* stmt = static_cast<sqlite3_stmt*>(stmt_insert_);
    
    // 重置 statement
    sqlite3_reset(stmt);
    
    // 绑定参数
    sqlite3_bind_text(stmt, 1, song_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, song_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, artist.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, local_path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 5, static_cast<int64_t>(std::time(nullptr)));
    
    // 执行插入
    int ret = sqlite3_step(stmt);
    if (ret != SQLITE_DONE) {
        syslog(LOG_ERR, "[ktv][db][error] component=history_db action=insert song_id=%s err=%s", 
               song_id.c_str(), sqlite3_errmsg(static_cast<sqlite3*>(db_)));
        return false;
    }
    
    // 立即裁剪到 max_count
    if (!trimToMaxCount()) {
        syslog(LOG_WARNING, "[ktv][db][warning] component=history_db action=trim_failed");
        // 不返回 false，因为插入已成功
    }
    
    syslog(LOG_DEBUG, "[ktv][db][action] component=history_db action=add_record song_id=%s", song_id.c_str());
    return true;
}

std::vector<HistoryDbItem> HistoryDbService::getHistoryList(int max_count) const {
    std::vector<HistoryDbItem> result;
    
    if (!initialized_) {
        syslog(LOG_ERR, "[ktv][db][error] component=history_db action=get_list reason=not_initialized");
        return result;
    }
    
    sqlite3_stmt* stmt = static_cast<sqlite3_stmt*>(stmt_select_);
    
    // 重置 statement
    sqlite3_reset(stmt);
    
    // 绑定参数
    sqlite3_bind_int(stmt, 1, max_count);
    
    // 执行查询
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        HistoryDbItem item;
        item.id = sqlite3_column_int64(stmt, 0);
        
        const unsigned char* text = sqlite3_column_text(stmt, 1);
        if (text) {
            item.song_id = reinterpret_cast<const char*>(text);
        }
        
        text = sqlite3_column_text(stmt, 2);
        if (text) {
            item.song_name = reinterpret_cast<const char*>(text);
        }
        
        text = sqlite3_column_text(stmt, 3);
        if (text) {
            item.artist = reinterpret_cast<const char*>(text);
        }
        
        text = sqlite3_column_text(stmt, 4);
        if (text) {
            item.local_path = reinterpret_cast<const char*>(text);
        }
        
        item.played_at = sqlite3_column_int64(stmt, 5);
        
        result.push_back(std::move(item));
    }
    
    return result;
}

bool HistoryDbService::clear() {
    if (!initialized_) {
        syslog(LOG_ERR, "[ktv][db][error] component=history_db action=clear reason=not_initialized");
        return false;
    }
    
    sqlite3_stmt* stmt = static_cast<sqlite3_stmt*>(stmt_clear_);
    
    sqlite3_reset(stmt);
    
    int ret = sqlite3_step(stmt);
    if (ret != SQLITE_DONE) {
        syslog(LOG_ERR, "[ktv][db][error] component=history_db action=clear err=%s", sqlite3_errmsg(static_cast<sqlite3*>(db_)));
        return false;
    }
    
    syslog(LOG_INFO, "[ktv][db][action] component=history_db action=clear");
    return true;
}

int HistoryDbService::getCount() const {
    if (!initialized_) {
        return 0;
    }
    
    sqlite3_stmt* stmt = static_cast<sqlite3_stmt*>(stmt_count_);
    
    sqlite3_reset(stmt);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        return sqlite3_column_int(stmt, 0);
    }
    
    return 0;
}

bool HistoryDbService::trimToMaxCount() {
    if (!initialized_) {
        return false;
    }
    
    sqlite3_stmt* stmt = static_cast<sqlite3_stmt*>(stmt_delete_);
    
    sqlite3_reset(stmt);
    sqlite3_bind_int(stmt, 1, max_count_);
    
    int ret = sqlite3_step(stmt);
    if (ret != SQLITE_DONE) {
        syslog(LOG_ERR, "[ktv][db][error] component=history_db action=trim err=%s", sqlite3_errmsg(static_cast<sqlite3*>(db_)));
        return false;
    }
    
    return true;
}

bool HistoryDbService::execSql(const char* sql) {
    if (!db_) {
        return false;
    }
    
    sqlite3* db = static_cast<sqlite3*>(db_);
    char* errmsg = nullptr;
    
    int ret = sqlite3_exec(db, sql, nullptr, nullptr, &errmsg);
    if (ret != SQLITE_OK) {
        syslog(LOG_ERR, "[ktv][db][error] component=history_db action=exec_sql err=%s", errmsg ? errmsg : "unknown");
        if (errmsg) {
            sqlite3_free(errmsg);
        }
        return false;
    }
    
    return true;
}

}  // namespace ktv::services

