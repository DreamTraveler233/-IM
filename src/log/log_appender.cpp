#include "log/log_appender.hpp"

#include <algorithm>

#include "base/macro.hpp"
#include "yaml-cpp/yaml.h"

namespace CIM {
LogAppender::~LogAppender() {}
void LogAppender::setFormatter(LogFormatter::ptr formatter) {
    CIM_ASSERT(formatter);
    MutexType::Lock lock(m_mutex);
    m_formatter = formatter;
}
LogFormatter::ptr LogAppender::getFormatter() const {
    MutexType::Lock lock(m_mutex);
    return m_formatter;
}
void LogAppender::setLevel(Level level) {
    CIM_ASSERT(level != Level::UNKNOWN);
    MutexType::Lock lock(m_mutex);
    m_level = level;
}
Level LogAppender::getLevel() const {
    MutexType::Lock lock(m_mutex);
    return m_level;
}

void StdoutLogAppender::log(LogEvent::ptr event) {
    CIM_ASSERT(event);
    if (event->getLevel() >= m_level) {
        MutexType::Lock lock(m_mutex);
        // 将日志事件（event）格式化后输出到标准输出（cout）
        std::cout << m_formatter->format(event);
    }
}

std::string StdoutLogAppender::toYamlString() {
    MutexType::Lock lock(m_mutex);
    YAML::Node node;
    node["type"] = "StdoutLogAppender";
    node["level"] = LogLevel::ToString(m_level);
    if (m_formatter) {
        node["formatter"] = m_formatter->getPattern();
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

FileLogAppender::FileLogAppender(const std::string& fileName) {
    CIM_ASSERT(!fileName.empty())
    m_logFile = LogFileManager::GetInstance()->getLogFile(fileName);
    m_logFile->openFile();
}

void FileLogAppender::log(LogEvent::ptr event) {
    CIM_ASSERT(event);
    if (event->getLevel() >= m_level) {
        MutexType::Lock lock(m_mutex);
        if (m_logFile) {
            std::string formatted_msg = m_formatter->format(event);
            if (m_logFile->getRotateType() == RotateType::SIZE) {
                const uint64_t threshold = m_logFile->getMaxFileSize();
                if (threshold > 0) {
                    const auto currentSize = m_logFile->getFileSize();
                    // 计算写入该日志消息后的预期文件大小
                    const uint64_t projectedSize =
                        static_cast<uint64_t>(std::max<int64_t>(0, currentSize)) +
                        formatted_msg.size();
                    if (projectedSize > threshold) {
                        LogFileManager::GetInstance()->rotateBySize(m_logFile);
                    }
                }
            }
            m_logFile->writeLog(formatted_msg);
        }
    }
}

std::string FileLogAppender::toYamlString() {
    MutexType::Lock lock(m_mutex);
    YAML::Node node;
    node["type"] = "FileLogAppender";
    node["file"] = m_logFile->getFilePath();
    node["level"] = LogLevel::ToString(m_level);
    if (m_formatter) {
        node["formatter"] = m_formatter->getPattern();
    }
    node["rotate_type"] = LogFile::rotateTypeToString(m_logFile->getRotateType());
    if (m_logFile->getRotateType() == RotateType::SIZE) {
        node["max_size"] = m_logFile->getMaxFileSize();
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

LogFile::ptr FileLogAppender::getLogFile() const {
    return m_logFile;
}
}  // namespace CIM