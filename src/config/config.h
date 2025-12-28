#ifndef KTVLV_CONFIG_CONFIG_H
#define KTVLV_CONFIG_CONFIG_H

#include <string>

namespace ktv::config {

struct NetworkConfig {
    std::string base_url = "https://mc.ktv.com.cn";
    int timeout = 10;
    std::string company = "mtc";
    std::string app_name = "pad1";
    std::string platform = "4.4";
    std::string vn = "1.0.0";
};

// 从 ini 文件加载配置，不存在则返回默认值；返回是否成功解析文件
bool loadFromFile(const std::string& path, NetworkConfig& out_cfg);

}  // namespace ktv::config

#endif  // KTVLV_CONFIG_CONFIG_H

