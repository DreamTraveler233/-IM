#include "log/log_level.hpp"

#include "base/macro.hpp"

namespace CIM {
const char* LogLevel::ToString(Level level) {
    switch (level) {
// 宏定义
#define XX(name)      \
    case Level::name: \
        return #name;

        XX(DEBUG);
        XX(INFO);
        XX(WARN);
        XX(ERROR);
        XX(FATAL);
// 取消宏定义
#undef XX
        default:
            return "UNKNOWN";
    }
}

Level LogLevel::FromString(const std::string& str) {
    CIM_ASSERT(!str.empty());
#define XX(name, level) \
    if (str == #name) return Level::level;

    XX(debug, DEBUG);
    XX(info, INFO);
    XX(warn, WARN);
    XX(error, ERROR);
    XX(fatal, FATAL);
    XX(Debug, DEBUG);
    XX(Info, INFO);
    XX(Warn, WARN);
    XX(Error, ERROR);
    XX(Fatal, FATAL);
    XX(DEBUG, DEBUG);
    XX(INFO, INFO);
    XX(WARN, WARN);
    XX(ERROR, ERROR);
    XX(FATAL, FATAL);
#undef XX
    return Level::UNKNOWN;
}
}  // namespace CIM