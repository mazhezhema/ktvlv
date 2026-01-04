#include "song_service.h"
#include "http_service.h"
#include "cJSON.h"
#include <syslog.h>

namespace ktv::services {

static void parse_song_array(const char* json_str, std::vector<SongItem>& out) {
    cJSON* root = cJSON_Parse(json_str);
    if (!root || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        return;
    }
    int n = cJSON_GetArraySize(root);
    for (int i = 0; i < n; ++i) {
        cJSON* item = cJSON_GetArrayItem(root, i);
        if (!item) continue;
        SongItem s;
        cJSON* id = cJSON_GetObjectItem(item, "song_id");
        cJSON* title = cJSON_GetObjectItem(item, "song_name");
        cJSON* artist = cJSON_GetObjectItem(item, "artist");
        cJSON* url = cJSON_GetObjectItem(item, "m3u8_url");
        if (id && cJSON_IsString(id)) s.id = id->valuestring;
        if (title && cJSON_IsString(title)) s.title = title->valuestring;
        if (artist && cJSON_IsString(artist)) s.artist = artist->valuestring;
        if (url && cJSON_IsString(url)) s.m3u8_url = url->valuestring;
        // fallback: if no song_id, use title as id
        if (s.id.empty()) s.id = s.title;
        if (!s.title.empty()) out.push_back(std::move(s));
    }
    cJSON_Delete(root);
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

