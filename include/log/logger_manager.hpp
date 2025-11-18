/**
 * @file logger_manager.hpp
 * @brief 日志管理器定义文件
 * @author CIM
 * @date 2025-10-21
 *
 * 该文件定义了日志管理器(LoggerManager)及相关配置结构体，
 * 用于统一管理和配置系统中的所有日志记录器(Logger)实例。
 */

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "io/lock.hpp"
#include "log_file.hpp"
#include "log_level.hpp"
#include "base/singleton.hpp"

namespace CIM {
class Logger;

/**
     * @brief 日志管理器类
     *
     * 负责管理系统中所有的日志记录器(Logger)实例，
     * 提供获取、创建和管理日志记录器的功能，并支持将配置序列化为YAML格式。
     */
class LoggerManager {
   public:
    using MutexType = Mutex;  ///< 互斥锁类型定义

    /**
         * @brief 构造函数
         */
    LoggerManager();

    /**
         * @brief 获取指定名称的日志记录器
         * @param[in] name 日志记录器名称
         * @return 日志记录器智能指针
         */
    std::shared_ptr<Logger> getLogger(const std::string& name);

    /**
         * @brief 获取根日志记录器
         * @return 根日志记录器智能指针
         */
    std::shared_ptr<Logger> getRoot() const;

    /**
         * @brief 将日志管理器配置转换为YAML字符串
         * @return YAML格式的配置字符串
         */
    std::string toYamlString();

   private:
    std::map<std::string, std::shared_ptr<Logger>> m_loggers;  ///< 日志记录器映射表
    std::shared_ptr<Logger> m_root;                            ///< 根日志记录器
    MutexType m_mutex;                                         ///< 互斥锁，保证线程安全
};

using LoggerMgr = CIM::Singleton<LoggerManager>;  ///< 日志管理器单例模式别名

/**
     * @brief 日志追加器配置项
     *
     * 定义日志追加器的基本配置信息，包括类型、级别、格式化器和文件路径等。
     */
struct LogAppenderDefine {
    int type = 0;                  ///< 追加器类型: 1-FileLogAppender, 2-StdoutLogAppender
    Level level = Level::UNKNOWN;  ///< 日志级别
    std::string formatter;         ///< 日志格式化器
    std::string path;              ///< 日志文件路径(仅FileLogAppender有效)
    RotateType rotateType = RotateType::NONE;  ///< 日志轮转类型(仅FileLogAppender有效)
    uint64_t maxFileSize = 0;                  ///< 文件大小轮转阈值(字节，文件追加器有效)

    /**
         * @brief 判断两个追加器配置是否相等
         * @param[in] other 另一个追加器配置
         * @return 是否相等
         */
    bool operator==(const LogAppenderDefine& other) const {
        return type == other.type && level == other.level && formatter == other.formatter &&
               path == other.path && rotateType == other.rotateType &&
               maxFileSize == other.maxFileSize;
    }
};

/**
     * @brief 日志配置项
     *
     * 定义日志记录器的基本配置信息，包括名称、级别、格式化器和追加器列表等。
     */
struct LogDefine {
    std::string name;                          ///< 日志记录器名称
    Level level = Level::UNKNOWN;              ///< 日志级别
    std::string formatter;                     ///< 日志格式化器
    std::vector<LogAppenderDefine> appenders;  ///< 日志追加器配置列表

    /**
         * @brief 判断两个日志配置是否相等
         * @param[in] other 另一个日志配置
         * @return 是否相等
         */
    bool operator==(const LogDefine& other) const {
        return name == other.name && level == other.level && formatter == other.formatter &&
               appenders == other.appenders;
    }

    /**
         * @brief 比较两个日志配置的大小关系(用于排序)
         * @param[in] other 另一个日志配置
         * @return 大小关系
         */
    bool operator<(const LogDefine& other) const { return name < other.name; }
};
}  // namespace CIM