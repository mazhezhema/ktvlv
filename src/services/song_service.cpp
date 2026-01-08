#include "song_service.h"
#include "http_service.h"
#include "utils/json_helper.h"
#include <syslog.h>

namespace ktv::services {

static bool is_ok_or_truncated(int ret) {
    // JsonHelper::GetString 可能返回 -5 表示 BufferTooSmall（截断），这在业务上可接受
    return (ret == 0 || ret == -5);
}

static void parse_song_array(const char* json_str, std::vector<SongItem>& out) {
    if (!json_str) return;

    ktv::utils::JsonDocument doc;
    int ret = JsonHelper::Parse(json_str, strlen(json_str), &doc);
    if (ret != 0) return;

    const cJSON* root = doc.root();
    ktv::utils::OutInt count;
    ret = JsonHelper::GetArraySize(root, &count);
    if (ret != 0) return;

    for (int i = 0; i < count.value; ++i) {
        SongItem s;

        char song_id[128]{0};
        char song_name[256]{0};
        char artist[256]{0};
        char m3u8_url[512]{0};

        int r_id = JsonHelper::GetRootArrayObjectString(root, i, "song_id", song_id, sizeof(song_id));
        int r_name = JsonHelper::GetRootArrayObjectString(root, i, "song_name", song_name, sizeof(song_name));
        int r_artist = JsonHelper::GetRootArrayObjectString(root, i, "artist", artist, sizeof(artist));
        int r_url = JsonHelper::GetRootArrayObjectString(root, i, "m3u8_url", m3u8_url, sizeof(m3u8_url));

        if (is_ok_or_truncated(r_id)) s.id = song_id;
        if (is_ok_or_truncated(r_name)) s.title = song_name;
        if (is_ok_or_truncated(r_artist)) s.artist = artist;
        if (is_ok_or_truncated(r_url)) s.m3u8_url = m3u8_url;

        // fallback: if no song_id, use title as id
        if (s.id.empty()) s.id = s.title;
        if (!s.title.empty()) out.push_back(std::move(s));
    }
}

std::vector<SongItem> SongService::listSongs(int page, int size) {
    std::vector<SongItem> result;
    HttpResponse resp;
    char url[512]{0};
    std::snprintf(url, sizeof(url),
                  "/kcloud/getmusics?token=%s&page=%d&size=%d&company=%s&app_name=%s&platform=%s&vn=%s",
                  token_.c_str(), page, size, net_cfg_.company.c_str(), net_cfg_.app_name.c_str(),
                  net_cfg_.platform.c_str(), net_cfg_.vn.c_str());
    if (!HttpService::getInstance().get(url, resp)) {
        syslog(LOG_WARNING, "[ktv][service][error] component=song_service action=list_songs reason=http_failed");
        return result;
    }
    parse_song_array(resp.body.data(), result);
    return result;
}

std::vector<SongItem> SongService::search(const std::string& keyword, int page, int size) {
    std::vector<SongItem> result;
    HttpResponse resp;
    char url[512]{0};
    std::snprintf(url, sizeof(url),
                  "/apollo/search/actorsong?token=%s&page=%d&size=%d&key=%s&company=%s&app_name=%s",
                  token_.c_str(), page, size, keyword.c_str(), net_cfg_.company.c_str(),
                  net_cfg_.app_name.c_str());
    if (!HttpService::getInstance().get(url, resp)) {
        syslog(LOG_WARNING, "[ktv][service][error] component=song_service action=search reason=http_failed");
        return result;
    }
    parse_song_array(resp.body.data(), result);
    return result;
}

bool SongService::addToQueue(const std::string& song_id) {
    HttpResponse resp;
    char url[512]{0};
    std::snprintf(url, sizeof(url),
                  "/karaoke_sdk/t/plist/set?token=%s",
                  token_.c_str());
    char body[256]{0};
    std::snprintf(body, sizeof(body), "{\"song_id\":\"%s\"}", song_id.c_str());
    if (!HttpService::getInstance().post(url, body, resp)) {
        syslog(LOG_WARNING, "[ktv][service][error] component=song_service action=add_to_queue reason=http_failed");
        return false;
    }
    return true;
}

}  // namespace ktv::services

