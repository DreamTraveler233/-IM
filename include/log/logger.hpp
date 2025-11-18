/**
 * @file logger.hpp
 * @brief 日志器类定义
 * @author CIM
 * @date 2025-10-21
 *
 * 该文件定义了日志器(Logger)类，负责管理日志的输出目标和日志级别的控制。
 * Logger是日志系统的核心组件之一，负责收集日志事件并将其分发给所有注册的
 * 日志输出器(LogAppender)。每个Logger可以设置独立的日志级别和格式化器。
 */

#pragma once

#include <list>
#include <memory>
#include <string>

#include "io/lock.hpp"
#include "log_appender.hpp"
#include "log_event.hpp"
#include "log_formatter.hpp"
#include "log_level.hpp"
#include "base/macro.hpp"

namespace CIM {

class LoggerManager;

/**
     * @brief 日志器类
     *
     * Logger是日志系统的中枢组件，负责管理日志的输出目标和日志级别的控制。
     * 它维护一个日志输出器列表，当日志事件到达时，会根据日志级别判断是否需要处理，
     * 如果需要处理，则将日志事件分发给所有注册的日志输出器。
     */
class Logger {
    friend class LoggerManager;

   public:
    /// 智能指针类型定义
    using ptr = std::shared_ptr<Logger>;
    /// 互斥锁类型定义
    using MutexType = Mutex;

    /**
         * @brief 构造函数
         * @param[in] name 日志器名称，默认为"root"
         */
    Logger(const std::string& name = "root");

    /**
         * @brief 记录日志
         * @param[in] level 日志级别
         * @param[in] event 日志事件
         */
    void log(Level level, LogEvent::ptr event);

    /**
         * @brief 记录DEBUG级别日志
         * @param[in] event 日志事件
         */
    void debug(LogEvent::ptr event);

    /**
         * @brief 记录INFO级别日志
         * @param[in] event 日志事件
         */
    void info(LogEvent::ptr event);

    /**
         * @brief 记录WARN级别日志
         * @param[in] event 日志事件
         */
    void warn(LogEvent::ptr event);

    /**
         * @brief 记录ERROR级别日志
         * @param[in] event 日志事件
         */
    void error(LogEvent::ptr event);

    /**
         * @brief 记录FATAL级别日志
         * @param[in] event 日志事件
         */
    void fatal(LogEvent::ptr event);

    /**
         * @brief 添加日志输出器
         * @param[in] appender 日志输出器
         */
    void addAppender(LogAppender::ptr appender);

    /**
         * @brief 删除日志输出器
         * @param[in] appender 日志输出器
         */
    void delAppender(LogAppender::ptr appender);

    /**
         * @brief 清空所有日志输出器
         */
    void clearAppender();

    /**
         * @brief 获取日志级别
         * @return 日志级别
         */
    Level getLevel() const;

    /**
         * @brief 设置日志级别
         * @param[in] level 日志级别
         */
    void setLevel(Level level);

    /**
         * @brief 获取日志器名称
         * @return 日志器名称
         */
    const std::string& getName() const;

    /**
         * @brief 设置日志格式器
         * @param[in] val 日志格式器
         */
    void setFormatter(LogFormatter::ptr val);

    /**
         * @brief 设置日志格式器
         * @param[in] val 日志格式器模式字符串
         */
    void setFormatter(const std::string& val);

    /**
         * @brief 获取日志格式器
         * @return 日志格式器
         */
    LogFormatter::ptr getFormatter() const;

    /**
         * @brief 获取根日志器
         * @return 根日志器
         */
    Logger::ptr getRoot() const;

    /**
         * @brief 将日志器配置转换为YAML格式字符串
         * @return YAML格式字符串
         */
    std::string toYamlString();

   private:
    std::string m_name;                       ///< 日志器名称
    Level m_level;                            ///< 日志级别
    LogFormatter::ptr m_formatter;            ///< 日志格式器
    std::list<LogAppender::ptr> m_appenders;  ///< 日志输出目标列表
    Logger::ptr m_root;                       ///< 根日志器
    MutexType m_mutex;                        ///< 互斥锁
};
}  // namespace CIM