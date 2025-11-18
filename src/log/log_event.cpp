#include "log/log_event.hpp"

#include <cstdarg>

#include "log/logger.hpp"

namespace CIM {
LogEvent::LogEvent(std::shared_ptr<Logger> logger, Level level, const char* file_name, int32_t line,
                   uint32_t elapse, uint32_t thread_id, uint32_t coroutine_id, uint64_t time,
                   const std::string& thread_name)
    : m_fileName(file_name),
      m_line(line),
      m_elapse(elapse),
      m_threadId(thread_id),
      m_threadName(thread_name),
      m_CoroutineId(coroutine_id),
      m_time(time),
      m_level(level),
      m_logger(logger) {}
const char* LogEvent::getFileName() const {
    return m_fileName;
}
int32_t LogEvent::getLine() const {
    return m_line;
}
uint32_t LogEvent::getElapse() const {
    return m_elapse;
}
uint32_t LogEvent::getThreadId() const {
    return m_threadId;
}
const std::string& LogEvent::getThreadName() const {
    return m_threadName;
}
uint32_t LogEvent::getCoroutineId() const {
    return m_CoroutineId;
}
uint64_t LogEvent::getTime() const {
    return m_time;
}
std::string LogEvent::getMessage() const {
    return m_messageSS.str();
}
std::stringstream& LogEvent::getSS() {
    return m_messageSS;
}
std::shared_ptr<Logger> LogEvent::getLogger() const {
    return m_logger;
}
Level LogEvent::getLevel() const {
    return m_level;
}

std::string LogEvent::getRelativeFileName() const {
    if (m_fileName == nullptr) {
        return "";
    }

    std::string file_name = m_fileName;
    // 查找文件路径中最后一个'/'的位置
    size_t pos = file_name.find_last_of('/');
    if (pos != std::string::npos) {
        // 查找 "CIM/" 子字符串
        size_t CIM_pos = file_name.find("CIM/");
        if (CIM_pos != std::string::npos) {
            return file_name.substr(CIM_pos + 4);
        }
        // 如果没有找到 "CIM/"，则返回文件名部分
        return file_name.substr(pos + 1);
    }
    return file_name;
}
void LogEvent::format(const char* fmt, ...) {
    CIM_ASSERT(fmt);
    va_list al;
    va_start(al, fmt);
    format(fmt, al);
    va_end(al);
}
void LogEvent::format(const char* fmt, va_list al) {
    CIM_ASSERT(fmt);
    char* buf = nullptr;
    int len = vasprintf(&buf, fmt, al);
    if (len != -1) {
        m_messageSS << std::string(buf, len);
        free(buf);
    }
}
LogEventWrap::LogEventWrap(LogEvent::ptr event) : m_event(event) {
    CIM_ASSERT(event);
}

LogEventWrap::~LogEventWrap() {
    m_event->getLogger()->log(m_event->getLevel(), m_event);
}
LogEvent::ptr LogEventWrap::getEvent() const {
    return m_event;
}
std::stringstream& LogEventWrap::getSS() {
    return m_event->getSS();
}
}  // namespace CIM