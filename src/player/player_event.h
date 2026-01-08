// player_event.h
#pragma once
#include <string>

enum class PlayerEventType {
    PREPARING,
    PLAYING,
    PAUSED,
    STOPPED,
    COMPLETED,
    ERROR
};

struct PlayerEvent {
    PlayerEventType type;
    int progress_ms = 0;
    int error_code = 0;
    std::string message;  // 可用于调试、UI提示
};




