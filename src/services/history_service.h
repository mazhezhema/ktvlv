#ifndef KTVLV_SERVICES_HISTORY_SERVICE_H
#define KTVLV_SERVICES_HISTORY_SERVICE_H

#include <vector>
#include <string>

namespace ktv::services {

/**
 * 历史记录项（兼容旧 API）
 */
struct HistoryItem {
    std::string title;
    std::string artist;
    std::string local_path;
    std::string song_id;  // 新增：歌曲ID（用于 SQLite）
};

/**
 * HistoryService - 历史记录服务（使用 SQLite）
 * 
 * 设计原则：
 * - Singleton 模式
 * - 内部使用 HistoryDbService 进行持久化
 * - 保持现有 API 不变（兼容性）
 * - 业务层无锁（HistoryDbService 内部处理）
 */
class HistoryService {
public:
    static HistoryService& getInstance() {
        static HistoryService instance;
        return instance;
    }
    
    HistoryService(const HistoryService&) = delete;
    HistoryService& operator=(const HistoryService&) = delete;
    
    /**
     * 初始化服务
     * @param db_path 数据库文件路径（如 "/data/ktv_history.db"）
     * @param max_count 最大记录数（默认 50）
     * @return true 成功，false 失败
     */
    bool initialize(const std::string& db_path = "/data/ktv_history.db", int max_count = 50);
    
    /**
     * 关闭服务
     */
    void shutdown();
    
    /**
     * 设置容量（兼容旧 API）
     * @param cap 最大记录数
     */
    void setCapacity(size_t cap);
    
    /**
     * 添加历史记录（兼容旧 API）
     * @param item 历史记录项
     */
    void add(const HistoryItem& item);
    
    /**
     * 获取历史记录列表（兼容旧 API）
     * @return 历史记录列表
     */
    std::vector<HistoryItem> items() const;
    
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

private:
    HistoryService() = default;
    ~HistoryService();
    
    bool initialized_ = false;
    int max_count_ = 50;
};

}  // namespace ktv::services

#endif  // KTVLV_SERVICES_HISTORY_SERVICE_H

