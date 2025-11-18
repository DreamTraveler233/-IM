#include "config/config.hpp"
#include "log/logger_manager.hpp"
#include "base/macro.hpp"

namespace CIM {
static auto g_logger = CIM_LOG_NAME("system");
// 用于存储所有的日志定义
auto g_log_defines = CIM::Config::Lookup("logs", std::set<LogDefine>(), "logs config");

// 静态变量初始化在main函数之前
struct LogInit {
    /**
         * @brief 日志配置变更监听回调函数
         *
         * 当日志配置发生变更时，此函数会被调用，负责根据新的配置更新所有日志器(Logger)的配置。
         * 包括新增、修改和删除日志器配置的操作。
         *
         * @param old_val 变更前的日志配置集合
         * @param new_val 变更后的日志配置集合
         */
    LogInit() {
        g_log_defines->addListener([](const std::set<LogDefine>& old_val,
                                      const std::set<LogDefine>& new_val) {
            CIM_LOG_INFO(g_logger) << "logger config changed";

            // 遍历新的日志配置，处理新增和修改的日志器
            for (auto& i : new_val) {
                // 查找日志器，如果找不到则创建并添加到日志器管理器中
                auto logger = CIM_LOG_NAME(i.name);

                // 设置该日志器的日志级别
                logger->setLevel(i.level);

                // 如果配置了格式化器，则设置日志格式
                if (!i.formatter.empty()) {
                    // 解析并设置格式化器，如果格式错误，则使用内置的默认格式
                    logger->setFormatter(i.formatter);
                }

                // 清除现有的日志输出器
                logger->clearAppender();

                // 遍历该日志器的所有输出器配置
                for (auto& j : i.appenders) {
                    LogAppender::ptr ap;

                    // 根据类型创建对应的日志输出器
                    if (j.type == 1) {
                        ap = std::make_shared<FileLogAppender>(j.path);
                    } else if (j.type == 2) {
                        ap = std::make_shared<StdoutLogAppender>();
                    } else {
                        CIM_LOG_ERROR(g_logger) << "type not FileLogAppender or StdoutLogAppender";
                    }

                    // 设置输出器的日志级别
                    if (j.level != Level::UNKNOWN) {
                        ap->setLevel(j.level);
                    } else {
                        ap->setLevel(i.level);
                    }

                    // 如果配置了特定的格式化器，则设置输出器的格式化器
                    if (!j.formatter.empty()) {
                        auto fmt = std::make_shared<LogFormatter>(j.formatter);
                        if (!fmt->isError()) {
                            ap->setFormatter(fmt);
                        }
                    }

                    // 如果配置了日志轮转类型，则设置文件日志输出器的轮转类型
                    if (j.rotateType != RotateType::NONE) {
                        auto fileAppender = std::dynamic_pointer_cast<FileLogAppender>(ap);
                        if (fileAppender) {
                            fileAppender->getLogFile()->setRotateType(j.rotateType);
                            fileAppender->getLogFile()->setMaxFileSize(j.maxFileSize);
                        }
                    }

                    // 将输出器添加到日志器中
                    logger->addAppender(ap);
                }
            }

            // 遍历旧的日志配置，处理已删除的日志器
            for (auto& i : old_val) {
                auto it = new_val.find(i);
                if (it == new_val.end()) {
                    // 如果在新配置中找不到当前旧配置项，说明这个配置项被删除了
                    auto logger = CIM_LOG_NAME(i.name);
                    logger->setLevel((Level)100);  // 设置日志级别为100，表示关闭日志
                    logger->clearAppender();       // 清空所有appender
                }
            }
        });
    }
};

static LogInit __log_init;
}  // namespace CIM