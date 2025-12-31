#include "event_bus.h"
#include <plog/Log.h>

namespace ktv::events {

void EventBus::publish(const Event& ev) {
    queue_.enqueue(ev);
}

bool EventBus::poll(Event& ev) {
    return queue_.try_dequeue(ev);
}

void EventBus::dispatchOnUiThread() {
    Event ev;
    // 从队列中取出所有待处理的事件
    while (poll(ev)) {
        // 根据事件类型进行分发处理
        // 注意：所有 UI 操作（lv_obj_xxx）必须在这里执行，确保在主线程
        switch (ev.type) {
            case EventType::SongSelected:
                PLOGI << "Event: SongSelected, payload=" << ev.payload;
                // TODO: 更新 UI（如播放列表、历史记录等）
                // 示例：更新播放列表 UI、添加到历史记录显示等
                break;
            case EventType::SongFavoriteToggle:
                PLOGI << "Event: SongFavoriteToggle, payload=" << ev.payload;
                // TODO: 更新收藏状态 UI
                break;
            case EventType::PageChange:
                PLOGI << "Event: PageChange, payload=" << ev.payload;
                // TODO: 切换页面（如果需要通过事件触发）
                break;
            case EventType::DownloadCompleted:
                PLOGI << "Event: DownloadCompleted, payload=" << ev.payload;
                // TODO: 更新下载状态 UI
                break;
            case EventType::PlayerStateChanged:
                PLOGI << "Event: PlayerStateChanged, payload=" << ev.payload;
                // TODO: 更新播放器状态 UI（播放/暂停按钮、进度条等）
                break;
            case EventType::LicenceStateChanged:
                PLOGI << "Event: LicenceStateChanged, payload=" << ev.payload;
                // TODO: 更新许可证状态 UI
                break;
            case EventType::None:
            default:
                PLOGW << "Unknown event type: " << static_cast<int>(ev.type);
                break;
        }
    }
}

}  // namespace ktv::events

