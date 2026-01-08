#ifndef KTVLV_SERVICES_HISTORY_SERVICE_H
#define KTVLV_SERVICES_HISTORY_SERVICE_H

#include <vector>
#include <string>

namespace ktv::services {

/**
 * 历史记录项（业务层使用）
 */
struct HistoryItem {
    std::string title;
    std::string artist;
    std::string local_path;
    std::string song_id;
};

/**
 * HistoryService - 历史记录服务
 * 
 * 设计原则：
 * - Singleton 模式
 * - 内部使用 HistoryDbService（SqliteHelper）
 * - 返回值表示状态（0 成功，<0 失败）
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
     * @param db_path 数据库文件路径
     * @param max_count 最大记录数（默认 50）
     * @return 0 成功；<0 失败
     */
    int initialize(const std::string& db_path = "/data/ktv_history.db", int max_count = 50);
    
    /**
     * 关闭服务
     */
    void shutdown();
    
    /**
     * 设置容量
     * @param cap 最大记录数
     */
    void setCapacity(size_t cap);
    
    /**
     * 添加历史记录
     * @param item 历史记录项
     * @return 0 成功；<0 失败
     */
    int add(const HistoryItem& item);
    
    /**
     * 获取历史记录列表
     * @param out_items 输出：历史记录列表
     * @return 0 成功；<0 失败
     */
    int getItems(std::vector<HistoryItem>& out_items) const;
    
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
    HistoryService() = default;
    ~HistoryService();
    
    bool initialized_ = false;
    int max_count_ = 50;
};

}  // namespace ktv::services

#endif  // KTVLV_SERVICES_HISTORY_SERVICE_H
