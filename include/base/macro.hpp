#pragma once

#include "log/logger.hpp"
#include "log/logger_manager.hpp"
#include "util/util.hpp"
#include "io/thread.hpp"
#include <string.h>
#include <assert.h>

/**
 * @brief 分支预测优化宏定义
 * @details 这些宏用于帮助编译器进行分支预测优化，提高程序执行效率
 * 在支持的编译器(GCC/LLVM)上使用__builtin_expect内建函数进行优化，
 * 在不支持的编译器上退化为普通表达式
 */
#if defined __GNUC__ || defined __llvm__
#define CIM_LIKELY(x) __builtin_expect(!!(x), 1)
#define CIM_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define CIM_LIKELY(x) (x)
#define CIM_UNLIKELY(x) (x)
#endif

#define CIM_LOG(logger, level)                                               \
    if (level >= logger->getLevel())                                           \
    CIM::LogEventWrap(                                                       \
        CIM::LogEvent::ptr(                                                  \
            new CIM::LogEvent(logger, level, __FILE__, __LINE__, 0,          \
                                CIM::GetThreadId(), CIM::GetCoroutineId(), \
                                time(0), CIM::Thread::GetName())))           \
        .getSS()

#define CIM_LOG_DEBUG(logger) CIM_LOG(logger, CIM::Level::DEBUG)
#define CIM_LOG_INFO(logger) CIM_LOG(logger, CIM::Level::INFO)
#define CIM_LOG_WARN(logger) CIM_LOG(logger, CIM::Level::WARN)
#define CIM_LOG_ERROR(logger) CIM_LOG(logger, CIM::Level::ERROR)
#define CIM_LOG_FATAL(logger) CIM_LOG(logger, CIM::Level::FATAL)

#define CIM_LOG_FMT(logger, level, fmt, ...)                                 \
    if (level >= logger->getLevel())                                           \
    CIM::LogEventWrap(                                                       \
        CIM::LogEvent::ptr(                                                  \
            new CIM::LogEvent(logger, level, __FILE__, __LINE__, 0,          \
                                CIM::GetThreadId(), CIM::GetCoroutineId(), \
                                time(0), CIM::Thread::GetName())))           \
        .getEvent()                                                            \
        ->format(fmt, __VA_ARGS__)

#define CIM_LOG_FMT_DEBUG(logger, fmt, ...) CIM_LOG_FMT(logger, CIM::Level::DEBUG, fmt, __VA_ARGS__)
#define CIM_LOG_FMT_INFO(logger, fmt, ...) CIM_LOG_FMT(logger, CIM::Level::INFO, fmt, __VA_ARGS__)
#define CIM_LOG_FMT_WARN(logger, fmt, ...) CIM_LOG_FMT(logger, CIM::Level::WARN, fmt, __VA_ARGS__)
#define CIM_LOG_FMT_ERROR(logger, fmt, ...) CIM_LOG_FMT(logger, CIM::Level::ERROR, fmt, __VA_ARGS__)
#define CIM_LOG_FMT_FATAL(logger, fmt, ...) CIM_LOG_FMT(logger, CIM::Level::FATAL, fmt, __VA_ARGS__)

#define CIM_LOG_ROOT() CIM::LoggerMgr::GetInstance()->getRoot()
#define CIM_LOG_NAME(name) CIM::LoggerMgr::GetInstance()->getLogger(name)

#define CIM_ASSERT(X)                                                                \
    if (CIM_UNLIKELY(!(X)))                                                          \
    {                                                                                  \
        CIM_LOG_ERROR(CIM_LOG_ROOT()) << "ASSERTION: " #X                          \
                                          << "\nbacktrace:\n"                          \
                                          << CIM::BacktraceToString(100, 2, "    "); \
        assert(X);                                                                     \
    }

#define CIM_ASSERT2(X, W)                                                            \
    if (CIM_UNLIKELY(!(X)))                                                          \
    {                                                                                  \
        CIM_LOG_ERROR(CIM_LOG_ROOT()) << "ASSERTION: " #X                          \
                                          << "\n"                                      \
                                          << W                                         \
                                          << "\nbacktrace:\n"                          \
                                          << CIM::BacktraceToString(100, 2, "    "); \
        assert(X);                                                                     \
    }
