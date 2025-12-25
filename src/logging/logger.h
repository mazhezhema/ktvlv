#ifndef KTVLV_LOGGING_LOGGER_H
#define KTVLV_LOGGING_LOGGER_H

#include <plog/Log.h>
#include <plog/Initializers/RollingFileInitializer.h>
#include <string>

namespace ktv::logging {

// 初始化日志：控制台 + 滚动文件
// log_path 为空时只输出控制台
void init(const std::string& log_path = "");

}  // namespace ktv::logging

#endif  // KTVLV_LOGGING_LOGGER_H

