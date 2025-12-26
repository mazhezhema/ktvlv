#ifndef KTVLV_SERVICES_QUEUE_SERVICE_H
#define KTVLV_SERVICES_QUEUE_SERVICE_H

#include <deque>
#include <string>
#include "song_service.h"

namespace ktv::services {

struct QueueItem {
    std::string song_id;
    std::string title;
    std::string artist;
    std::string m3u8_url;
};

class QueueService {
public:
    static QueueService& getInstance() {
        static QueueService instance;
        return instance;
    }
    QueueService(const QueueService&) = delete;
    QueueService& operator=(const QueueService&) = delete;

    // 添加歌曲到队列
    void add(const QueueItem& item);
    
    // 获取队列
    const std::deque<QueueItem>& getQueue() const { return queue_; }
    
    // 获取当前播放的歌曲索引
    int getCurrentIndex() const { return current_index_; }
    
    // 设置当前播放索引
    void setCurrentIndex(int index);
    
    // 获取下一首
    QueueItem* getNext();
    
    // 获取当前歌曲
    QueueItem* getCurrent();
    
    // 删除指定索引的歌曲
    void remove(int index);
    
    // 清空队列
    void clear();
    
    // 队列是否为空
    bool empty() const { return queue_.empty(); }
    
    // 队列大小
    size_t size() const { return queue_.size(); }

private:
    QueueService() = default;
    ~QueueService() = default;

    std::deque<QueueItem> queue_;
    int current_index_{-1};  // -1 表示没有正在播放的歌曲
};

}  // namespace ktv::services

#endif  // KTVLV_SERVICES_QUEUE_SERVICE_H


