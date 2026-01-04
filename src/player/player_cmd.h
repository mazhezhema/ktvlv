// player_cmd.h
#pragma once
#include <string>

enum class PlayerCmdType {
    PLAY,           // 播放/切歌
    PAUSE,          // 暂停
    RESUME,         // 继续
    REPLAY,         // 重唱（seek 0）
    SWITCH_TRACK,   // 原/伴奏切换
    SET_VOLUME,     // 调音量
    STOP,           // 停止本首
    EXIT            // 退出播放器（释放资源）
};

struct PlayerCmd {
    PlayerCmdType type;
    std::string url;  // PLAY 用
    int value = 0;    // SET_VOLUME / SWITCH_TRACK 用
};


