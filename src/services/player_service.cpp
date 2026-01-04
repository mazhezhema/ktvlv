#include "player_service.h"
#include <syslog.h>
#include "../events/event_bus.h"

namespace ktv::services {

void PlayerService::play(const std::string& song_id, const std::string& m3u8_url) {
    syslog(LOG_INFO, "[ktv][player][action] action=play song_id=%s url=%s status=mock", song_id.c_str(), m3u8_url.c_str());
    state_ = PlayerState::Playing;
    ktv::events::Event ev;
    ev.type = ktv::events::EventType::PlayerStateChanged;
    ev.payload = "playing";
    ktv::events::EventBus::getInstance().publish(ev);
}

void PlayerService::pause() {
    if (state_ == PlayerState::Playing) {
        state_ = PlayerState::Paused;
        syslog(LOG_INFO, "[ktv][player][action] action=pause");
    }
}

void PlayerService::resume() {
    if (state_ == PlayerState::Paused) {
        state_ = PlayerState::Playing;
        syslog(LOG_INFO, "[ktv][player][action] action=resume");
    }
}

void PlayerService::stop() {
    if (state_ != PlayerState::Stopped) {
        state_ = PlayerState::Stopped;
        syslog(LOG_INFO, "[ktv][player][action] action=stop");
    }
}

}  // namespace ktv::services

