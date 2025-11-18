/**
 * @file log_formatter.hpp
 * @brief 日志格式化器头文件
 * @author CIM
 * @date 2025-10-21
 *
 * 定义了日志格式化器(LogFormatter)及相关格式化项类。
 * LogFormatter负责将日志事件(LogEvent)按照指定的格式模式转换为字符串输出。
 * 支持多种格式化项，如时间、线程ID、日志级别等，可以灵活组合形成不同的日志输出格式。
 */

#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace CIM {
class LogEvent;

/**
     * @class LogFormatter
     * @brief 日志格式化器类
     *
     * 将日志事件按照指定的模式格式化为字符串。
     */
class LogFormatter {
   public:
    /// 智能指针类型定义
    using ptr = std::shared_ptr<LogFormatter>;

    /**
         * @brief 构造函数
         * @param[in] pattern 格式化模式字符串
         */
    LogFormatter(const std::string& pattern);

    /**
         * @brief 格式化日志事件
         * @param[in] event 日志事件智能指针
         * @return 格式化后的字符串
         */
    std::string format(std::shared_ptr<LogEvent> event);

   public:
    /**
         * @class FormatItem
         * @brief 格式化项基类
         *
         * 所有具体的格式化项都需要继承此类并实现format方法。
         * 每个格式化项负责处理一种特定的格式化内容。
         */
    class FormatItem {
       public:
        /// 智能指针类型定义
        using ptr = std::shared_ptr<FormatItem>;

        /**
             * @brief 默认构造函数
             */
        FormatItem() = default;

        /**
             * @brief 虚析构函数
             */
        virtual ~FormatItem() = default;

        /**
             * @brief 格式化日志事件内容到输出流
             * @param[in] os 输出流
             * @param[in] event 日志事件智能指针
             */
        virtual void format(std::ostream& os, std::shared_ptr<LogEvent> event) = 0;
    };

    /**
         * @brief 初始化解析格式模式
         */
    void init();

    /**
         * @brief 检查解析是否出错
         * @return 是否出错
         */
    bool isError() const;

    /**
         * @brief 获取格式模式字符串
         * @return 格式模式字符串
         */
    const std::string& getPattern() const;

   private:
    std::string m_pattern;                 ///< 格式模式字符串
    std::vector<FormatItem::ptr> m_items;  ///< 格式化项列表
    bool m_isError;                        ///< 是否解析出错标志
};

/**
     * @class MessageFormatItem
     * @brief 消息内容格式化项
     *
     * 负责格式化日志消息内容，对应格式符%m
     */
class MessageFormatItem : public LogFormatter::FormatItem {
   public:
    /**
         * @brief 默认构造函数
         */
    MessageFormatItem() = default;

    /**
         * @brief 格式化日志事件内容到输出流
         * @param[in] os 输出流
         * @param[in] event 日志事件智能指针
         */
    void format(std::ostream& os, std::shared_ptr<LogEvent> event) override;
};

/**
     * @class LevelFormatItem
     * @brief 日志级别格式化项
     *
     * 负责格式化日志级别，对应格式符%p
     */
class LevelFormatItem : public LogFormatter::FormatItem {
   public:
    /**
         * @brief 默认构造函数
         */
    LevelFormatItem() = default;

    /**
         * @brief 格式化日志事件内容到输出流
         * @param[in] os 输出流
         * @param[in] event 日志事件智能指针
         */
    void format(std::ostream& os, std::shared_ptr<LogEvent> event) override;
};

/**
     * @class ElapseFormatItem
     * @brief 运行时间格式化项
     *
     * 负责格式化程序启动到现在的耗时，对应格式符%r
     */
class ElapseFormatItem : public LogFormatter::FormatItem {
   public:
    /**
         * @brief 默认构造函数
         */
    ElapseFormatItem() = default;

    /**
         * @brief 格式化日志事件内容到输出流
         * @param[in] os 输出流
         * @param[in] event 日志事件智能指针
         */
    void format(std::ostream& os, std::shared_ptr<LogEvent> event) override;
};

/**
     * @class NameFormatItem
     * @brief 日志器名称格式化项
     *
     * 负责格式化日志器名称，对应格式符%c
     */
class NameFormatItem : public LogFormatter::FormatItem {
   public:
    /**
         * @brief 默认构造函数
         */
    NameFormatItem() = default;

    /**
         * @brief 格式化日志事件内容到输出流
         * @param[in] os 输出流
         * @param[in] event 日志事件智能指针
         */
    void format(std::ostream& os, std::shared_ptr<LogEvent> event) override;
};

/**
     * @class ThreadIdFormatItem
     * @brief 线程ID格式化项
     *
     * 负责格式化线程ID，对应格式符%t
     */
class ThreadIdFormatItem : public LogFormatter::FormatItem {
   public:
    /**
         * @brief 默认构造函数
         */
    ThreadIdFormatItem() = default;

    /**
         * @brief 格式化日志事件内容到输出流
         * @param[in] os 输出流
         * @param[in] event 日志事件智能指针
         */
    void format(std::ostream& os, std::shared_ptr<LogEvent> event) override;
};

/**
     * @class ThreadNameFormatItem
     * @brief 线程名称格式化项
     *
     * 负责格式化线程名称，对应格式符%N
     */
class ThreadNameFormatItem : public LogFormatter::FormatItem {
   public:
    /**
         * @brief 默认构造函数
         */
    ThreadNameFormatItem() = default;

    /**
         * @brief 格式化日志事件内容到输出流
         * @param[in] os 输出流
         * @param[in] event 日志事件智能指针
         */
    void format(std::ostream& os, std::shared_ptr<LogEvent> event) override;
};

/**
     * @class DateTimeFormatItem
     * @brief 时间格式化项
     *
     * 负责格式化时间戳，对应格式符%d
     */
class DateTimeFormatItem : public LogFormatter::FormatItem {
   public:
    /**
         * @brief 默认构造函数
         */
    DateTimeFormatItem();

    /**
         * @brief 格式化日志事件内容到输出流
         * @param[in] os 输出流
         * @param[in] event 日志事件智能指针
         */
    void format(std::ostream& os, std::shared_ptr<LogEvent> event) override;

   private:
    std::string m_format;  ///< 时间格式字符串
};

/**
     * @class FileNameFormatItem
     * @brief 文件名格式化项
     *
     * 负责格式化源文件名，对应格式符%f
     */
class FileNameFormatItem : public LogFormatter::FormatItem {
   public:
    /**
         * @brief 默认构造函数
         */
    FileNameFormatItem() = default;

    /**
         * @brief 格式化日志事件内容到输出流
         * @param[in] os 输出流
         * @param[in] event 日志事件智能指针
         */
    void format(std::ostream& os, std::shared_ptr<LogEvent> event) override;
};

/**
     * @class LineFormatItem
     * @brief 行号格式化项
     *
     * 负责格式化源代码行号，对应格式符%l
     */
class LineFormatItem : public LogFormatter::FormatItem {
   public:
    /**
         * @brief 默认构造函数
         */
    LineFormatItem() = default;

    /**
         * @brief 格式化日志事件内容到输出流
         * @param[in] os 输出流
         * @param[in] event 日志事件智能指针
         */
    void format(std::ostream& os, std::shared_ptr<LogEvent> event) override;
};

/**
     * @class NewLineFormatItem
     * @brief 换行符格式化项
     *
     * 负责输出换行符，对应格式符%n
     */
class NewLineFormatItem : public LogFormatter::FormatItem {
   public:
    /**
         * @brief 默认构造函数
         */
    NewLineFormatItem() = default;

    /**
         * @brief 格式化日志事件内容到输出流
         * @param[in] os 输出流
         * @param[in] event 日志事件智能指针
         */
    void format(std::ostream& os, std::shared_ptr<LogEvent> event) override;
};

/**
     * @class TabFormatItem
     * @brief 制表符格式化项
     *
     * 负责输出制表符，对应格式符%T
     */
class TabFormatItem : public LogFormatter::FormatItem {
   public:
    /**
         * @brief 默认构造函数
         */
    TabFormatItem() = default;

    /**
         * @brief 格式化日志事件内容到输出流
         * @param[in] os 输出流
         * @param[in] event 日志事件智能指针
         */
    void format(std::ostream& os, std::shared_ptr<LogEvent> event) override;
};

/**
     * @class FiberIdFormatItem
     * @brief 协程ID格式化项
     *
     * 负责格式化协程ID，对应格式符%F
     */
class FiberIdFormatItem : public LogFormatter::FormatItem {
   public:
    /**
         * @brief 默认构造函数
         */
    FiberIdFormatItem() = default;

    /**
         * @brief 格式化日志事件内容到输出流
         * @param[in] os 输出流
         * @param[in] event 日志事件智能指针
         */
    void format(std::ostream& os, std::shared_ptr<LogEvent> event) override;
};

/**
     * @class StringFormatItem
     * @brief 字符串格式化项
     *
     * 负责输出普通字符串，主要用于处理格式字符串中的普通文本部分
     */
class StringFormatItem : public LogFormatter::FormatItem {
   public:
    /**
         * @brief 构造函数
         * @param[in] str 字符串内容
         */
    StringFormatItem(const std::string& str);

    /**
         * @brief 格式化日志事件内容到输出流
         * @param[in] os 输出流
         * @param[in] event 日志事件智能指针
         */
    void format(std::ostream& os, std::shared_ptr<LogEvent> event) override;

   private:
    std::string m_string;  ///< 字符串内容
};
}  // namespace CIM