// sqlite_helper.cpp
// SQLite 单例辅助类实现
#include "sqlite_helper.h"
#include "log_macros.h"
#include <sqlite3.h>

namespace ktv::utils {

// 进程级唯一 DB 连接
static sqlite3* g_db = nullptr;

int SqliteHelper::Init(const char* db_path) {
    if (g_db) {
        // 已初始化，幂等
        return 0;
    }

    if (!db_path) {
        KTV_LOG_ERR("db", "action=init reason=null_path");
        return -1;
    }

    int rc = sqlite3_open(db_path, &g_db);
    if (rc != SQLITE_OK) {
        KTV_LOG_ERR("db", "action=open path=%s err=%s", db_path, 
                    g_db ? sqlite3_errmsg(g_db) : "unknown");
        if (g_db) {
            sqlite3_close(g_db);
            g_db = nullptr;
        }
        return -1;
    }

    // 嵌入式 / 单进程推荐配置
    sqlite3_exec(g_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(g_db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(g_db, "PRAGMA temp_store=MEMORY;", nullptr, nullptr, nullptr);
    sqlite3_exec(g_db, "PRAGMA cache_size=-512;", nullptr, nullptr, nullptr);  // 512KB

    KTV_LOG_INFO("db", "action=init path=%s", db_path);
    return 0;
}

void SqliteHelper::Shutdown() {
    if (g_db) {
        sqlite3_close(g_db);
        g_db = nullptr;
        KTV_LOG_INFO("db", "action=shutdown");
    }
}

int SqliteHelper::Exec(const char* sql) {
    if (!g_db) {
        KTV_LOG_ERR("db", "action=exec reason=not_initialized");
        return -1;
    }
    if (!sql) {
        return -1;
    }

    char* err = nullptr;
    int rc = sqlite3_exec(g_db, sql, nullptr, nullptr, &err);

    if (rc != SQLITE_OK) {
        KTV_LOG_ERR("db", "action=exec err=%s sql=%.64s", err ? err : "unknown", sql);
        if (err) {
            sqlite3_free(err);
        }
        return -1;
    }

    return 0;
}

int SqliteHelper::Query(const char* sql, std::vector<SqlRow>& rows) {
    rows.clear();

    if (!g_db) {
        KTV_LOG_ERR("db", "action=query reason=not_initialized");
        return -1;
    }
    if (!sql) {
        return -1;
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        KTV_LOG_ERR("db", "action=prepare err=%s sql=%.64s", sqlite3_errmsg(g_db), sql);
        return -1;
    }

    int col_count = sqlite3_column_count(stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SqlRow row;
        row.cols.reserve(static_cast<size_t>(col_count));

        for (int i = 0; i < col_count; ++i) {
            const unsigned char* v = sqlite3_column_text(stmt, i);
            row.cols.emplace_back(v ? reinterpret_cast<const char*>(v) : "");
        }
        rows.emplace_back(std::move(row));
    }

    sqlite3_finalize(stmt);
    return 0;
}

bool SqliteHelper::IsInitialized() {
    return g_db != nullptr;
}

}  // namespace ktv::utils

