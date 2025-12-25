#ifndef KTVLV_SERVICES_PLAYER_SERVICE_H
#define KTVLV_SERVICES_PLAYER_SERVICE_H

#include <string>

namespace ktv::services {

enum class PlayerState {
    Stopped = 0,
    Playing,
    Paused
};

class PlayerService {
public:
    static PlayerService& getInstance() {
        static PlayerService instance;
        return instance;
    }
    PlayerService(const PlayerService&) = delete;
    PlayerService& operator=(const PlayerService&) = delete;

    void play(const std::string& song_id, const std::string& m3u8_url);
    void pause();
    void resume();
    void stop();
    PlayerState state() const { return state_; }

private:
    PlayerService() = default;
    ~PlayerService() = default;

    PlayerState state_{PlayerState::Stopped};
};

}  // namespace ktv::services

#endif  // KTVLV_SERVICES_PLAYER_SERVICE_H

