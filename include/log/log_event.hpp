/**
 * @file log_event.hpp
 * @brief 日志事件相关类定义
 * @author CIM
 * @date 2025-10-21
 *
 * 该文件定义了日志事件相关的类，包括LogEvent和LogEventWrap。
 * LogEvent用于封装一次日志记录的所有相关信息，如文件名、行号、线程ID等。
 * LogEventWrap是一个包装类，用于在析构时自动将日志事件提交到日志系统。
 */

#pragma once

#include <memory>
#include <sstream>

#include "log_level.hpp"

namespace CIM {
class Logger;

/**
     * @brief 日志事件类
     *
     * 封装一次日志记录的所有相关信息，包括源文件名、行号、线程ID、协程ID、
     * 时间戳等元信息，以及日志消息内容。
     */
class LogEvent {
   public:
    /// 智能指针类型定义
    using ptr = std::shared_ptr<LogEvent>;

    /**
         * @brief 构造函数
         * @param[in] logger 关联的日志器
         * @param[in] level 日志等级
         * @param[in] file_name 源文件名
         * @param[in] line 行号
         * @param[in] elapse 程序启动到现在的耗时(毫秒)
         * @param[in] thread_id 线程ID
         * @param[in] fiber_id 协程ID
         * @param[in] time 时间戳
         * @param[in] thread_name 线程名称
         */
    LogEvent(std::shared_ptr<Logger> logger, Level level, const char* file_name, int32_t line,
             uint32_t elapse, uint32_t thread_id, uint32_t fiber_id, uint64_t time,
             const std::string& thread_name);

    /**
         * @brief 获取源文件名
         * @return 源文件名
         */
    const char* getFileName() const;

    /**
         * @brief 获取相对文件名（去除路径前缀）
         * @return 相对文件名
         */
    std::string getRelativeFileName() const;

    /**
         * @brief 获取行号
         * @return 行号
         */
    int32_t getLine() const;

    /**
         * @brief 获取耗时
         * @return 程序启动到现在的耗时(毫秒)
         */
    uint32_t getElapse() const;

    /**
         * @brief 获取线程ID
         * @return 线程ID
         */
    uint32_t getThreadId() const;

    /**
         * @brief 获取线程名称
         * @return 线程名称
         */
    const std::string& getThreadName() const;

    /**
         * @brief 获取协程ID
         * @return 协程ID
         */
    uint32_t getCoroutineId() const;

    /**
         * @brief 获取时间戳
         * @return 时间戳
         */
    uint64_t getTime() const;

    /**
         * @brief 获取日志消息内容
         * @return 日志消息内容
         */
    std::string getMessage() const;

    /**
         * @brief 获取消息内容流
         * @return 消息内容流
         */
    std::stringstream& getSS();

    /**
         * @brief 获取关联的日志器
         * @return 关联的日志器
         */
    std::shared_ptr<Logger> getLogger() const;

    /**
         * @brief 获取日志等级
         * @return 日志等级
         */
    Level getLevel() const;

    /**
         * @brief 格式化写入日志内容
         * @param[in] fmt 格式字符串
         * @param[in] ... 可变参数
         */
    void format(const char* fmt, ...);

    /**
         * @brief 格式化写入日志内容
         * @param[in] fmt 格式字符串
         * @param[in] al 可变参数列表
         */
    void format(const char* fmt, va_list al);

   private:
    const char* m_fileName = nullptr;  ///< 源文件名
    int32_t m_line = 0;                ///< 行号
    uint32_t m_elapse = 0;             ///< 程序启动到现在的耗时(毫秒)
    uint32_t m_threadId = 0;           ///< 线程ID
    std::string m_threadName;          ///< 线程名称
    uint32_t m_CoroutineId = 0;        ///< 协程ID
    uint64_t m_time;                   ///< 时间戳
    std::stringstream m_messageSS;     ///< 日志消息流
    Level m_level;                     ///< 日志等级
    std::shared_ptr<Logger> m_logger;  ///< 关联的日志器
};

/**
     * @brief 日志事件包装类
     *
     * 包装LogEvent，在析构时自动将日志事件提交到关联的日志器中，
     * 实现日志的自动记录功能。
     */
class LogEventWrap {
   public:
    /**
         * @brief 构造函数
         * @param[in] event 日志事件
         */
    LogEventWrap(LogEvent::ptr event);

    /**
         * @brief 析构函数
         *
         * 在析构时将日志事件提交到日志系统
         */
    ~LogEventWrap();

    /**
         * @brief 获取日志事件
         * @return 日志事件
         */
    LogEvent::ptr getEvent() const;

    /**
         * @brief 获取日志内容流
         * @return 日志内容流
         */
    std::stringstream& getSS();

   private:
    LogEvent::ptr m_event;  ///< 日志事件
};

}  // namespace CIM