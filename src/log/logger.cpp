#include "log/logger.hpp"

#include "yaml-cpp/yaml.h"

namespace CIM {
Logger::Logger(const std::string& name) : m_name(name), m_level(Level::DEBUG) {
    CIM_ASSERT(!name.empty());
    // 时间 线程名称 线程号 协程号 [日志级别] [日志器名称] <文件名:行号> 日志信息 回车
    m_formatter = std::make_shared<LogFormatter>("%d%T%N%T%t%T%F%T[%p]%T[%c]%T<%f:%l>%T%m%n");
}

/**
     * 日志分发机制：
     *      1、首先检查日志级别是否达到记录标准
     *      2、如果有附加器(appender)，则遍历所有附加器记录日志
     *      3、如果没有附加器但有根日志器，则使用根日志器记录日志
     */
void Logger::log(Level level, LogEvent::ptr event) {
    CIM_ASSERT(event);
    if (level >= m_level) {
        std::list<LogAppender::ptr> appenders;
        Logger::ptr root;
        {
            // 采取锁分离，仅在需要时获取锁，然后立即释放，防止死锁的可能
            MutexType::Lock lock(m_mutex);
            appenders = m_appenders;
            root = m_root;
        }
        // 如果有附加器，则遍历所有附加器记录日志
        if (!appenders.empty()) {
            for (auto& i : appenders) {
                i->log(event);
            }
        }
        // 如果没有附加器但有根日志器，则使用根日志器记录
        else if (root) {
            root->log(level, event);
        }
    }
}

void Logger::debug(LogEvent::ptr event) {
    log(Level::DEBUG, event);
}
void Logger::info(LogEvent::ptr event) {
    log(Level::INFO, event);
}
void Logger::warn(LogEvent::ptr event) {
    log(Level::WARN, event);
}
void Logger::error(LogEvent::ptr event) {
    log(Level::ERROR, event);
}
void Logger::fatal(LogEvent::ptr event) {
    log(Level::FATAL, event);
}

void Logger::addAppender(LogAppender::ptr appender) {
    CIM_ASSERT(appender);
    MutexType::Lock lock(m_mutex);
    // 当appender没有自己的formatter时，则使用logger的formatter
    if (!appender->getFormatter()) {
        appender->setFormatter(m_formatter);
    }
    m_appenders.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender) {
    CIM_ASSERT(appender);
    MutexType::Lock lock(m_mutex);
    for (auto it = m_appenders.begin(); it != m_appenders.end(); ++it) {
        if (*it == appender) {
            m_appenders.erase(it);
            break;
        }
    }
}

void Logger::clearAppender() {
    MutexType::Lock lock(m_mutex);
    m_appenders.clear();
}
Level Logger::getLevel() const {
    MutexType::Lock lock(m_mutex);
    return m_level;
}
void Logger::setLevel(Level level) {
    CIM_ASSERT(level != Level::UNKNOWN);
    MutexType::Lock lock(m_mutex);
    m_level = level;
}
const std::string& Logger::getName() const {
    MutexType::Lock lock(m_mutex);
    return m_name;
}
void Logger::setFormatter(LogFormatter::ptr val) {
    CIM_ASSERT(val);
    MutexType::Lock lock(m_mutex);
    m_formatter = val;
}
/**
     * 该函数根据传入的格式化字符串创建一个新的LogFormatter对象，
     * 并检查该格式化器是否有效。如果有效，则将其设置为当前Logger的格式化器；
     * 如果无效，则输出错误信息并返回。
     */
void Logger::setFormatter(const std::string& val) {
    CIM_ASSERT(!val.empty());
    auto new_val = std::make_shared<LogFormatter>(val);
    if (new_val->isError()) {
        std::cout << "Logger setFormatter name=" << m_name << " value=" << val
                  << " invalid formatter" << std::endl;
        return;
    }
    setFormatter(new_val);
}
LogFormatter::ptr Logger::getFormatter() const {
    MutexType::Lock lock(m_mutex);
    return m_formatter;
}
Logger::ptr Logger::getRoot() const {
    MutexType::Lock lock(m_mutex);
    return m_root;
}
std::string Logger::toYamlString() {
    MutexType::Lock lock(m_mutex);
    YAML::Node node;
    node["name"] = m_name;
    node["level"] = LogLevel::ToString(m_level);
    if (m_formatter) {
        node["formatter"] = m_formatter->getPattern();
    }
    for (auto& i : m_appenders) {
        node["appenders"].push_back(YAML::Load(i->toYamlString()));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}
}  // namespace CIM