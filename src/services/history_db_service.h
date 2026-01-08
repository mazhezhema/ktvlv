#ifndef KTVLV_SERVICES_HISTORY_DB_SERVICE_H
#define KTVLV_SERVICES_HISTORY_DB_SERVICE_H

#include <string>
#include <vector>
#include <cstdint>

namespace ktv::services {

/**
 * 历史记录项
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
 * HistoryDbService - 历史记录数据库服务
 * 
 * 设计原则：
 * - Singleton 模式
 * - 内部使用 SqliteHelper（进程唯一 DB）
 * - 50/100 条上限，每次插入后立即裁剪
 * - 返回值表示状态（0 成功，<0 失败）
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
     * 初始化数据库（调用 SqliteHelper::Init + 建表）
     * @param db_path 数据库文件路径
     * @param max_count 最大记录数（默认 50）
     * @return 0 成功；<0 失败
     */
    int initialize(const std::string& db_path, int max_count = 50);
    
    /**
     * 关闭数据库
     */
    void shutdown();
    
    /**
     * 添加播放记录
     * @return 0 成功；<0 失败
     */
    int addRecord(const std::string& song_id,
                  const std::string& song_name,
                  const std::string& artist,
                  const std::string& local_path = "");
    
    /**
     * 获取历史记录列表（按播放时间倒序）
     * @param out_items 输出：历史记录列表
     * @param max_count 最大返回数量
     * @return 0 成功；<0 失败
     */
    int getHistoryList(std::vector<HistoryDbItem>& out_items, int max_count = 50) const;
    
    /**
     * 清空所有历史记录
     * @return 0 成功；<0 失败
     */
    int clear();
    
    /**
     * 获取记录总数
     * @param out_count 输出：记录总数
     * @return 0 成功；<0 失败
     */
    int getCount(int& out_count) const;
    
    /**
     * 检查是否已初始化
     */
    bool isInitialized() const { return initialized_; }

private:
    HistoryDbService() = default;
    ~HistoryDbService();
    
    /**
     * 裁剪记录到 max_count 条
     * @return 0 成功；<0 失败
     */
    int trimToMaxCount();
    
    /**
     * 转义 SQL 字符串中的单引号
     */
    static std::string escapeSql(const std::string& str);
    
    bool initialized_ = false;
    int max_count_ = 50;
};

}  // namespace ktv::services

#endif  // KTVLV_SERVICES_HISTORY_DB_SERVICE_H
