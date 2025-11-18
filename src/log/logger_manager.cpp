#include "log/logger_manager.hpp"

#include "log/log_appender.hpp"
#include "log/logger.hpp"
#include "yaml-cpp/yaml.h"

namespace CIM {
LoggerManager::LoggerManager() {
    // 创建默认的根日志器，name-root，level-DEBUG，formatter-"%d%T[thread-%t]%T[coroutine-%F]%T[%p]%T[name-%c]%T<%f:%l>%T%n%m%n"
    m_root = std::make_shared<Logger>();
    // 为根日志器添加日志输出目标
    m_root->addAppender(LogAppender::ptr(std::make_shared<StdoutLogAppender>()));
    // 添加到日志器管理中
    m_loggers[m_root->getName()] = m_root;
}

std::shared_ptr<Logger> LoggerManager::getLogger(const std::string& name) {
    CIM_ASSERT(!name.empty());
    MutexType::Lock lock(m_mutex);
    auto it = m_loggers.find(name);  // 查找指定名称的日志器
    if (it != m_loggers.end()) {
        // 如果找到则直接返回
        return it->second;
    }
    // 如果未找到，则创建新的日志器，并将其根设置为m_root
    auto logger = std::make_shared<Logger>(name);
    logger->m_root = m_root;
    m_loggers[name] = logger;
    return logger;
}

std::shared_ptr<Logger> LoggerManager::getRoot() const {
    return m_root;
}

std::string LoggerManager::toYamlString() {
    MutexType::Lock lock(m_mutex);
    YAML::Node node;
    for (auto& i : m_loggers) {
        node.push_back(YAML::Load(i.second->toYamlString()));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}
}  // namespace CIM