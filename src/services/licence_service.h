#ifndef KTVLV_SERVICES_LICENCE_SERVICE_H
#define KTVLV_SERVICES_LICENCE_SERVICE_H

#include <string>

namespace ktv::services {

enum class LicenceStatus {
    NotActivated = 0,
    Trial,
    Activated,
    Expired,
    Invalid
};

struct LicenceInfo {
    std::string licence_code;
    std::string mac_address;
    LicenceStatus status{LicenceStatus::NotActivated};
    int trial_count{0};
};

class LicenceService {
public:
    static LicenceService& getInstance() {
        static LicenceService instance;
        return instance;
    }
    LicenceService(const LicenceService&) = delete;
    LicenceService& operator=(const LicenceService&) = delete;

    bool initialize();
    bool verify(const std::string& licence_code);
    LicenceStatus getStatus() const { return info_.status; }
    
    // 通过 license 获取 token（需要 company、app_name 和 macid 参数）
    std::string getTokenFromLicense(const std::string& license, 
                                     const std::string& company = "", 
                                     const std::string& app_name = "",
                                     const std::string& macid = "");
    
    // 获取运行时配置（需要在获取 token 后调用）
    bool getRuntimeConfig(const std::string& token,
                          const std::string& platform,
                          const std::string& company,
                          const std::string& app_name,
                          const std::string& vn);
    
    // 检查更新（可选，根据 hot_update 标志决定是否调用）
    std::string checkUpdate(const std::string& token,
                            const std::string& platform,
                            const std::string& vn,
                            const std::string& license,
                            const std::string& company,
                            const std::string& app_name);
    
    // 获取当前 license
    const std::string& getLicense() const { return info_.licence_code; }
    
    // 获取设备 MAC 地址（作为 macid）
    static std::string getMacAddress();

private:
    LicenceService() = default;
    ~LicenceService() = default;

    LicenceInfo info_;
};

}  // namespace ktv::services

#endif  // KTVLV_SERVICES_LICENCE_SERVICE_H

