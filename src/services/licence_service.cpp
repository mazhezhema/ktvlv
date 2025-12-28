#include "licence_service.h"
#include <plog/Log.h>

namespace ktv::services {

bool LicenceService::initialize() {
    // 占位：实际应加载本地文件
    info_.status = LicenceStatus::NotActivated;
    return true;
}

bool LicenceService::verify(const std::string& licence_code) {
    // 占位：实际应调用服务器验证
    info_.licence_code = licence_code;
    info_.status = LicenceStatus::Trial;
    PLOGI << "Licence verify mock, code=" << licence_code;
    return true;
}

}  // namespace ktv::services

