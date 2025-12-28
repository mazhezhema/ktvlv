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

private:
    LicenceService() = default;
    ~LicenceService() = default;

    LicenceInfo info_;
};

}  // namespace ktv::services

#endif  // KTVLV_SERVICES_LICENCE_SERVICE_H

