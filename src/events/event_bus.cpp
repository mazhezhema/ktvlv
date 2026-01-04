#include "event_bus.h"
#include <syslog.h>

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
                syslog(LOG_INFO, "[ktv][event][song_selected] payload=%s", ev.payload.c_str());
                // TODO: 更新 UI（如播放列表、历史记录等）
                // 示例：更新播放列表 UI、添加到历史记录显示等
                break;
            case EventType::SongFavoriteToggle:
                syslog(LOG_INFO, "[ktv][event][song_favorite_toggle] payload=%s", ev.payload.c_str());
                // TODO: 更新收藏状态 UI
                break;
            case EventType::PageChange:
                syslog(LOG_INFO, "[ktv][event][page_change] payload=%s", ev.payload.c_str());
                // TODO: 切换页面（如果需要通过事件触发）
                break;
            case EventType::DownloadCompleted:
                syslog(LOG_INFO, "[ktv][event][download_completed] payload=%s", ev.payload.c_str());
                // TODO: 更新下载状态 UI
                break;
            case EventType::PlayerStateChanged:
                syslog(LOG_INFO, "[ktv][event][player_state_changed] payload=%s", ev.payload.c_str());
                // TODO: 更新播放器状态 UI（播放/暂停按钮、进度条等）
                break;
            case EventType::LicenceStateChanged:
                syslog(LOG_INFO, "[ktv][event][licence_state_changed] payload=%s", ev.payload.c_str());
                // TODO: 更新许可证状态 UI
                break;
            case EventType::None:
            default:
                syslog(LOG_WARNING, "[ktv][event][unknown] type=%d", static_cast<int>(ev.type));
                break;
        }
    }
}

}  // namespace ktv::events

