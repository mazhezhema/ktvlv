#include "m3u8_download_service.h"
#include <syslog.h>
#include "../events/event_bus.h"

namespace ktv::services {

void M3u8DownloadService::startDownload(const std::string& song_id, const std::string& m3u8_url) {
    syslog(LOG_INFO, "[ktv][download][action] action=start song_id=%s url=%s status=mock", song_id.c_str(), m3u8_url.c_str());
    // mock: immediately publish completed
    ktv::events::Event ev;
    ev.type = ktv::events::EventType::DownloadCompleted;
    ev.payload = song_id;
    ktv::events::EventBus::getInstance().publish(ev);
}

void M3u8DownloadService::process() {
    // mock: nothing to process
}

}  // namespace ktv::services

