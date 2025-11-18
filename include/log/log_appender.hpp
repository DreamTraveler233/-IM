/**
 * @file log_appender.hpp
 * @brief 日志追加器定义文件
 * @author CIM
 * @date 2025-10-21
 *
 * 该文件定义了日志系统的核心组件——日志追加器(LogAppender)基类及其派生类，
 * 包括标准输出追加器和文件追加器，用于控制日志的输出目标和格式化方式。
 */

#pragma once

#include <fstream>
#include <string>

#include "io/lock.hpp"
#include "log_event.hpp"
#include "log_file.hpp"
#include "log_file_manager.hpp"
#include "log_formatter.hpp"

namespace CIM {
/**
     * @brief 日志追加器基类
     *
     * 定义日志追加器的接口规范，所有具体的日志追加器都需要继承此类。
     * 负责管理日志级别、格式化器以及线程安全等功能。
     */
class LogAppender {
   public:
    using ptr = std::shared_ptr<LogAppender>;  ///< 智能指针类型定义
    using MutexType = Mutex;                   ///< 互斥锁类型定义

    /**
         * @brief 析构函数
         */
    virtual ~LogAppender();

    /**
         * @brief 写入日志事件
         * @param[in] event 日志事件
         */
    virtual void log(LogEvent::ptr event) = 0;

    /**
         * @brief 将追加器配置转换为YAML字符串
         * @return YAML格式的配置字符串
         */
    virtual std::string toYamlString() = 0;

    /**
         * @brief 设置日志格式器
         * @param[in] formatter 日志格式器
         */
    void setFormatter(LogFormatter::ptr formatter);

    /**
         * @brief 获取日志格式器
         * @return 日志格式器
         */
    LogFormatter::ptr getFormatter() const;

    /**
         * @brief 设置日志级别
         * @param[in] level 日志级别
         */
    void setLevel(Level level);

    /**
         * @brief 获取日志级别
         * @return 日志级别
         */
    Level getLevel() const;

   protected:
    Level m_level = Level::DEBUG;   ///< 日志级别，默认为DEBUG
    LogFormatter::ptr m_formatter;  ///< 日志格式器
    MutexType m_mutex;              ///< 互斥锁，保证线程安全
};

/**
     * @brief 标准输出日志追加器
     *
     * 将日志输出到标准输出流(std::cout)
     */
class StdoutLogAppender : public LogAppender {
   public:
    using ptr = std::shared_ptr<StdoutLogAppender>;  ///< 智能指针类型定义

    /**
         * @brief 写入日志事件到标准输出
         * @param[in] event 日志事件
         */
    void log(LogEvent::ptr event) override;

    /**
         * @brief 将追加器配置转换为YAML字符串
         * @return YAML格式的配置字符串
         */
    std::string toYamlString() override;
};

/**
     * @brief 文件日志追加器
     *
     * 将日志输出到指定文件中
     */
class FileLogAppender : public LogAppender {
   public:
    using ptr = std::shared_ptr<FileLogAppender>;  ///< 智能指针类型定义

    /**
         * @brief 构造函数
         * @param[in] fileName 日志文件名
         */
    FileLogAppender(const std::string& fileName);

    /**
         * @brief 析构函数
         */
    ~FileLogAppender() = default;

    /**
         * @brief 写入日志事件到文件
         * @param[in] event 日志事件
         */
    void log(LogEvent::ptr event) override;

    /**
         * @brief 将追加器配置转换为YAML字符串
         * @return YAML格式的配置字符串
         */
    std::string toYamlString() override;

    /**
         * @brief 获取日志文件对象
         * @return 日志文件智能指针
         */
    LogFile::ptr getLogFile() const;

   private:
    LogFile::ptr m_logFile;  ///< 文件对象
};
}  // namespace CIM