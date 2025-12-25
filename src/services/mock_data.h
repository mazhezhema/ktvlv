#ifndef KTVLV_SERVICES_MOCK_DATA_H
#define KTVLV_SERVICES_MOCK_DATA_H

#include <vector>
#include <string>

namespace ktv::mock {

struct SongItem {
    std::string title;
    std::string artist;
};

const std::vector<SongItem>& hotSongs();
const std::vector<SongItem>& historySongs();
std::vector<SongItem> searchSongs(const std::string& keyword);

}  // namespace ktv::mock

#endif  // KTVLV_SERVICES_MOCK_DATA_H

