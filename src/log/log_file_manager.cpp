#include "log/log_file_manager.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "base/macro.hpp"
#include "util/string_util.hpp"
#include "util/time_util.hpp"

namespace CIM {
static auto g_logger = CIM_LOG_NAME("system");

LogFileManager::LogFileManager()
    : m_lastYear(-1),
      m_lastMonth(-1),
      m_lastDay(-1),
      m_lastHour(-1),
      m_lastMinute(-1),
      m_isInit(false) {
    init();
}

LogFileManager::~LogFileManager() {
    if (m_timer) {
        m_timer->cancel();
    }
}

void LogFileManager::init() {
    if (m_isInit) {
        return;
    }
    m_isInit = true;

    IOManager* iom = IOManager::GetThis();
    if (iom) {
        m_timer = iom->addTimer(1000, [this]() { onCheck(); }, true);
    }
}

void LogFileManager::onCheck() {
    // 时间改变标志
    bool day_change{false};
    bool hour_change{false};
    bool minute_change{false};
    bool second_change{false};

    // 获取当前时间
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    TimeUtil::Now(year, month, day, hour, minute, second);

    // 如果是第一次检查
    if (-1 == m_lastYear && -1 == m_lastMonth && -1 == m_lastDay && -1 == m_lastHour &&
        -1 == m_lastMinute) {
        m_lastYear = year;
        m_lastMonth = month;
        m_lastDay = day;
        m_lastHour = hour;
        m_lastMinute = minute;
        return;  // 直接返回
    }

    // 判断天、小时、分钟是否变化
    if (m_lastMinute != minute)  // 过了一分钟
    {
        minute_change = true;
    }
    if (m_lastHour != hour)  // 过了一小时
    {
        hour_change = true;
    }
    if (m_lastDay != day)  // 过了一天
    {
        day_change = true;
    }

    // 如果没有变化则直接返回
    if (!day_change && !hour_change && !minute_change && !second_change) {
        return;
    }

    MutexType::Lock lock(m_mutex);
    // 遍历所有日志文件，根据轮转类型进行轮转
    for (auto& log : m_logs) {
        if (minute_change && log.second->getRotateType() == RotateType::MINUTE) {
            rotateMinute(log.second);
        }
        if (hour_change && log.second->getRotateType() == RotateType::HOUR) {
            rotateHours(log.second);
        }
        if (day_change && log.second->getRotateType() == RotateType::DAY) {
            rotateDays(log.second);
        }
    }

    // 更新上次时间
    m_lastYear = year;
    m_lastMonth = month;
    m_lastDay = day;
    m_lastHour = hour;
    m_lastMinute = minute;
}

LogFile::ptr LogFileManager::getLogFile(const std::string& fileName) {
    CIM_ASSERT(!fileName.empty())
    MutexType::Lock lock(m_mutex);
    // 查找日志文件
    auto it = m_logs.find(fileName);
    if (it != m_logs.end()) {
        // 查找到日志文件，返回该文件的指针
        return it->second;
    }

    // 未查找到日志文件
    LogFile::ptr log = std::make_shared<LogFile>(fileName);
    m_logs[fileName] = log;
    return log;
}

void LogFileManager::rotateBySize(const LogFile::ptr& file) {
    const uint64_t threshold = file->getMaxFileSize();
    if (threshold == 0) {
        return;
    }

    auto currentSize = file->getFileSize();
    if (static_cast<uint64_t>(std::max<int64_t>(0, currentSize)) < threshold) {
        return;
    }

    MutexType::Lock lock(m_mutex);
    currentSize = file->getFileSize();
    if (static_cast<uint64_t>(std::max<int64_t>(0, currentSize)) < threshold) {
        return;
    }

    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    TimeUtil::Now(year, month, day, hour, minute, second);
    const uint64_t micro = TimeUtil::NowToUS();

    char buf[160] = {0};
    snprintf(buf, sizeof(buf), "_%04d-%02d-%02dT%02d%02d%02d.%06lu", year, month, day, hour, minute,
             second, static_cast<unsigned long>(micro % 1000000));

    const std::string file_path = file->getFilePath();
    const std::string path = StringUtil::FilePath(file_path);
    const std::string file_name = StringUtil::FileName(file_path);
    const std::string file_ext = StringUtil::Extension(file_path);

    std::ostringstream ss;
    ss << path << file_name << buf << file_ext;
    file->rotate(ss.str());
}

void LogFileManager::rotateMinute(const LogFile::ptr& file) {
    if (file->getFileSize() > 0) {
        char buf[120] = {0};
        // 文件名后缀格式：_YYYY-MM-DDTHHMM
        sprintf(buf, "_%04d-%02d-%02dT%02d%02d", m_lastYear, m_lastMonth, m_lastDay, m_lastHour,
                m_lastMinute);

        std::string file_path = file->getFilePath();
        // 获取文件路径
        std::string path = StringUtil::FilePath(file_path);
        // 获取文件名
        std::string file_name = StringUtil::FileName(file_path);
        // 获取扩展名
        std::string file_ext = StringUtil::Extension(file_path);

        std::ostringstream ss;
        ss << path       // ../log/
           << file_name  // tmms
           << buf        // _YYYY-MM-DDTHHMM
           << file_ext;  // .log
        file->rotate(ss.str());
    }
}

void LogFileManager::rotateHours(const LogFile::ptr& file) {
    if (file->getFileSize() > 0) {
        char buf[120] = {0};
        // 文件名后缀格式：_YYYY-MM-DDTHH
        sprintf(buf, "_%04d-%02d-%02dT%02d", m_lastYear, m_lastMonth, m_lastDay, m_lastHour);

        std::string file_path = file->getFilePath();
        // 获取文件路径
        std::string path = StringUtil::FilePath(file_path);
        // 获取文件名
        std::string file_name = StringUtil::FileName(file_path);
        // 获取扩展名
        std::string file_ext = StringUtil::Extension(file_path);

        std::ostringstream ss;
        ss << path << file_name << buf << file_ext;
        file->rotate(ss.str());
    }
}

void LogFileManager::rotateDays(const LogFile::ptr& file) {
    if (file->getFileSize() > 0) {
        char buf[120] = {0};
        // 文件名后缀格式：_YYYY-MM-DD
        sprintf(buf, "_%04d-%02d-%02d", m_lastYear, m_lastMonth, m_lastDay);

        std::string file_path = file->getFilePath();
        // 获取文件路径
        std::string path = StringUtil::FilePath(file_path);
        // 获取文件名
        std::string file_name = StringUtil::FileName(file_path);
        // 获取扩展名
        std::string file_ext = StringUtil::Extension(file_path);

        std::ostringstream ss;
        ss << path << file_name << buf << file_ext;
        file->rotate(ss.str());
    }
}
}  // namespace CIM