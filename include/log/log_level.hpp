/**
 * @file log_level.hpp
 * @brief 日志等级定义及转换接口
 * @author CIM
 * @date 2025-10-21
 * 
 * 该文件定义了日志系统的日志等级枚举和相关转换函数，
 * 包括从日志等级到字符串的转换和从字符串到日志等级的转换。
 * 日志等级从低到高依次为: DEBUG, INFO, WARN, ERROR, FATAL
 */

#pragma once

#include <string>

namespace CIM {
/**
     * @brief 日志等级枚举
     *
     * 使用枚举类定义日志等级，避免命名冲突
     * 等级数值越大，优先级越高
     */
enum class Level {
    UNKNOWN = 0,  ///< 未知等级
    DEBUG = 1,    ///< 调试信息等级
    INFO = 2,     ///< 普通信息等级
    WARN = 3,     ///< 警告信息等级
    ERROR = 4,    ///< 错误信息等级
    FATAL = 5     ///< 致命错误等级
};

/**
     * @brief 日志等级类
     *
     * 提供日志等级的枚举定义以及相关转换函数。
     * 日志等级从低到高依次为: DEBUG, INFO, WARN, ERROR, FATAL
     */
class LogLevel {
   public:
    /**
         * @brief 将日志等级转换为字符串表示
         * @param[in] level 日志等级
         * @return 对应的字符串表示
         */
    static const char* ToString(Level level);

    /**
         * @brief 将字符串转换为日志等级
         * @param[in] str 等级字符串（不区分大小写）
         * @return 对应的日志等级枚举值，无法识别时返回UNKNOWN
         */
    static Level FromString(const std::string& str);
};

}  // namespace CIM