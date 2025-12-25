#include "player_service.h"
#include <plog/Log.h>
#include "../events/event_bus.h"

namespace ktv::services {

void PlayerService::play(const std::string& song_id, const std::string& m3u8_url) {
    PLOGI << "Play (mock) song=" << song_id << " url=" << m3u8_url;
    state_ = PlayerState::Playing;
    ktv::events::Event ev;
    ev.type = ktv::events::EventType::PlayerStateChanged;
    ev.payload = "playing";
    ktv::events::EventBus::getInstance().publish(ev);
}

void PlayerService::pause() {
    if (state_ == PlayerState::Playing) {
        state_ = PlayerState::Paused;
        PLOGI << "Pause";
    }
}

void PlayerService::resume() {
    if (state_ == PlayerState::Paused) {
        state_ = PlayerState::Playing;
        PLOGI << "Resume";
    }
}

void PlayerService::stop() {
    if (state_ != PlayerState::Stopped) {
        state_ = PlayerState::Stopped;
        PLOGI << "Stop";
    }
}

}  // namespace ktv::services

