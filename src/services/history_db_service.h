#ifndef KTVLV_SERVICES_HISTORY_DB_SERVICE_H
#define KTVLV_SERVICES_HISTORY_DB_SERVICE_H

#include <string>
#include <vector>
#include <cstdint>

namespace ktv::services {

/**
 * 历史记录项（与 HistoryService 兼容）
 */
struct HistoryDbItem {
    int64_t id = 0;              // 数据库自增ID
    std::string song_id;         // 歌曲ID
    std::string song_name;       // 歌曲名称
    std::string artist;          // 歌手
    std::string local_path;      // 本地文件路径（可选）
    int64_t played_at = 0;       // 播放时间（unix timestamp）
};

/**
 * HistoryDbService - SQLite 历史记录服务
 * 
 * 设计原则：
 * - Singleton 模式
 * - 50/100 条上限，每次插入后立即裁剪
 * - 单 DB 线程（通过消息队列，符合业务线程模型）
 * - 使用 prepared statement
 * - WAL 模式
 * - 极简设计，避免过度封装
 */
class HistoryDbService {
public:
    static HistoryDbService& instance() {
        static HistoryDbService inst;
        return inst;
    }
    
    HistoryDbService(const HistoryDbService&) = delete;
    HistoryDbService& operator=(const HistoryDbService&) = delete;
    
    /**
     * 初始化数据库
     * @param db_path 数据库文件路径（如 "/data/ktv_history.db"）
     * @param max_count 最大记录数（默认 50）
     * @return true 成功，false 失败
     */
    bool initialize(const std::string& db_path, int max_count = 50);
    
    /**
     * 关闭数据库
     */
    void shutdown();
    
    /**
     * 添加播放记录
     * @param song_id 歌曲ID
     * @param song_name 歌曲名称
     * @param artist 歌手
     * @param local_path 本地文件路径（可选）
     * @return true 成功，false 失败
     * 
     * 注意：插入后会自动裁剪到 max_count 条
     */
    bool addRecord(const std::string& song_id,
                   const std::string& song_name,
                   const std::string& artist,
                   const std::string& local_path = "");
    
    /**
     * 获取历史记录列表（按播放时间倒序）
     * @param max_count 最大返回数量（默认 50）
     * @return 历史记录列表
     */
    std::vector<HistoryDbItem> getHistoryList(int max_count = 50) const;
    
    /**
     * 清空所有历史记录
     * @return true 成功，false 失败
     */
    bool clear();
    
    /**
     * 获取记录总数
     * @return 记录总数
     */
    int getCount() const;
    
    /**
     * 检查是否已初始化
     * @return true 已初始化，false 未初始化
     */
    bool isInitialized() const { return initialized_; }

private:
    HistoryDbService() = default;
    ~HistoryDbService();
    
    /**
     * 裁剪记录到 max_count 条（保留最新的）
     * @return true 成功，false 失败
     */
    bool trimToMaxCount();
    
    /**
     * 执行 SQL（用于初始化）
     * @param sql SQL 语句
     * @return true 成功，false 失败
     */
    bool execSql(const char* sql);
    
    bool initialized_ = false;
    struct sqlite3* db_ = nullptr;  // sqlite3*（前向声明，避免暴露 sqlite3.h）
    int max_count_ = 50;
    std::string db_path_;
    
    // Prepared statements（避免重复编译）
    struct sqlite3_stmt* stmt_insert_ = nullptr;
    struct sqlite3_stmt* stmt_select_ = nullptr;
    struct sqlite3_stmt* stmt_delete_ = nullptr;
    struct sqlite3_stmt* stmt_count_ = nullptr;
    struct sqlite3_stmt* stmt_clear_ = nullptr;
};

}  // namespace ktv::services

#endif  // KTVLV_SERVICES_HISTORY_DB_SERVICE_H

