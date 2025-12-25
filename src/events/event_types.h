#ifndef KTVLV_EVENTS_EVENT_TYPES_H
#define KTVLV_EVENTS_EVENT_TYPES_H

#include <string>

namespace ktv::events {

enum class EventType {
    None = 0,
    SongSelected,
    SongFavoriteToggle,
    PageChange,
    DownloadCompleted,
    PlayerStateChanged,
    LicenceStateChanged,
};

struct Event {
    EventType type{EventType::None};
    std::string payload;  // 简易序列化：如 song_id 或 JSON 字符串
};

}  // namespace ktv::events

#endif  // KTVLV_EVENTS_EVENT_TYPES_H

