#include "m3u8_download_service.h"
#include <plog/Log.h>
#include "../events/event_bus.h"

namespace ktv::services {

void M3u8DownloadService::startDownload(const std::string& song_id, const std::string& m3u8_url) {
    PLOGI << "Start download (mock) song=" << song_id << " url=" << m3u8_url;
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

