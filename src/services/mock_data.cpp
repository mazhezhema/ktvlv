#include "mock_data.h"

namespace ktv::mock {

// 默认数据：包含完整字段，作为离线时的默认填充数据
static const std::vector<SongItem> kHotSongs = {
    {"红日", "李克勤"},
    {"上海滩", "叶丽仪"},
    {"朋友", "周华健"},
    {"遥远的她", "张学友"},
    {"单身情歌", "林志炫"},
    {"匆匆那年", "王菲"},
    {"演员", "薛之谦"},
    {"平凡之路", "朴树"},
};

static const std::vector<SongItem> kHistorySongs = {
    {"海阔天空", "Beyond"},
    {"吻别", "张学友"},
    {"光辉岁月", "Beyond"},
    {"飘雪", "陈慧娴"},
    {"十年", "陈奕迅"},
};

const std::vector<SongItem>& hotSongs() {
    return kHotSongs;
}

const std::vector<SongItem>& historySongs() {
    return kHistorySongs;
}

std::vector<SongItem> searchSongs(const std::string& keyword) {
    std::vector<SongItem> result;
    for (const auto& s : kHotSongs) {
        if (s.title.find(keyword) != std::string::npos ||
            s.artist.find(keyword) != std::string::npos) {
            result.push_back(s);
        }
    }
    return result;
}

}  // namespace ktv::mock

