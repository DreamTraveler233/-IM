/**
 * @file log_file_manager.hpp
 * @brief 日志文件管理模块
 *
 * 该模块提供日志文件的统一管理功能，包括日志文件的创建、轮转和清理。
 * 支持按分钟、小时、天三种时间单位进行日志轮转，通过定时器自动检测时间变化并执行轮转操作。
 *
 * 主要功能：
 * 1. 统一管理所有日志文件对象
 * 2. 自动按时间轮转日志文件
 * 3. 线程安全的操作接口
 *
 * 使用方式：
 * 通过getSingleton获取单例对象，然后使用getLogFile获取或创建日志文件对象。
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "io/iomanager.hpp"
#include "io/lock.hpp"
#include "log_file.hpp"
#include "base/singleton.hpp"
#include "io/timer.hpp"

namespace CIM {
class LogFileManager : public Singleton<LogFileManager> {
   public:
    using ptr = std::shared_ptr<LogFileManager>;
    using MutexType = Mutex;

    /**
         * @brief 构造函数
         *
         * 初始化成员变量，并调用init()方法进行初始化
         */
    LogFileManager();

    /**
         * @brief 析构函数
         *
         * 取消定时器，释放相关资源
         */
    ~LogFileManager();

    /**
         * @brief 获取指定文件名的日志文件对象，如果不存在则创建。
         *
         * @param fileName 日志文件名（含路径）
         * @return LogFilePtr 日志文件智能指针，打开失败时返回空指针
         */
    LogFile::ptr getLogFile(const std::string& fileName);

    /**
         * @brief 按文件大小轮转日志文件。
         *
         * @param file 日志文件智能指针
         */
    void rotateBySize(const LogFile::ptr& file);

   private:
    /**
         * @brief 初始化日志文件管理器
         *
         * 初始化定时器，每秒执行一次onCheck()方法检查是否需要轮转日志文件
         */
    void init();

    /**
         * @brief 检查日志文件是否需要轮转（按分钟、小时、天），并执行轮转操作。
         *
         * 该函数会根据当前时间与上次记录的时间进行比较，判断是否发生了天、小时或分钟的变化。
         * 如果发生变化，则对相应类型的日志文件进行轮转。
         */
    void onCheck();

    /**
         * @brief 按分钟进行日志轮转。
         *
         * @param file 日志文件智能指针
         */
    void rotateMinute(const LogFile::ptr& file);

    /**
         * @brief 按小时进行日志轮转。
         *
         * @param file 日志文件智能指针
         */
    void rotateHours(const LogFile::ptr& file);

    /**
         * @brief 按天进行日志轮转。
         *
         * @param file 日志文件智能指针
         */
    void rotateDays(const LogFile::ptr& file);

   private:
    std::unordered_map<std::string, LogFile::ptr> m_logs;  //!< 文件路径+文件名->日志文件对象的映射
    MutexType m_mutex;                                     //!< 线程安全锁
    int m_lastYear;                                        //!< 上次检查时的年份
    int m_lastMonth;                                       //!< 上次检查时的月份
    int m_lastDay;                                         //!< 上次检查时的日期
    int m_lastHour;                                        //!< 上次检查时的小时
    int m_lastMinute;                                      //!< 上次检查时的分钟
    Timer::ptr m_timer;                                    //!< 定时器，用于定时检查是否需要轮转
    bool m_isInit;                                         //!< 是否已初始化标志
};
}  // namespace CIM