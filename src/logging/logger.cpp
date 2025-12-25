#include "logger.h"
#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Appenders/RollingFileAppender.h>

namespace ktv::logging {

void init(const std::string& log_path) {
    // 控制台日志
    static plog::ConsoleAppender<plog::TxtFormatter> console_appender;
    plog::init(plog::debug, &console_appender);

    if (!log_path.empty()) {
        static plog::RollingFileAppender<plog::TxtFormatter> file_appender(
            log_path.c_str(), 512 * 1024, 3);
        plog::get()->addAppender(&file_appender);
    }
}

}  // namespace ktv::logging

