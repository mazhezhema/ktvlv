#include "config.h"
#include <ini.h>
#include <cstdlib>

namespace ktv::config {

static int handler(void* user, const char* section, const char* name, const char* value) {
    NetworkConfig* cfg = static_cast<NetworkConfig*>(user);
    std::string sec(section);
    std::string key(name);
    std::string val(value ? value : "");

    if (sec == "network") {
        if (key == "base_url") cfg->base_url = val;
        else if (key == "timeout") cfg->timeout = std::atoi(val.c_str());
        else if (key == "company") cfg->company = val;
        else if (key == "app_name") cfg->app_name = val;
        else if (key == "platform") cfg->platform = val;
        else if (key == "vn") cfg->vn = val;
        else if (key == "license") cfg->license = val;
    }
    return 1;
}

bool loadFromFile(const std::string& path, NetworkConfig& out_cfg) {
    int ret = ini_parse(path.c_str(), handler, &out_cfg);
    return ret == 0;
}

}  // namespace ktv::config

