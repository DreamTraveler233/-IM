#include "log/logger.hpp"
#include "log/logger_manager.hpp"
#include "config/config.hpp"
#include "log/log_appender.hpp"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <cassert>
#include <fstream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>

// 测试日志系统
void test_log_system()
{
    std::cout << "=================== 日志系统基本 ===================" << std::endl;

    auto system_log = CIM_LOG_NAME("system");
    auto root_log = CIM_LOG_NAME("root");

    CIM_LOG_DEBUG(system_log) << "debug message for system";
    CIM_LOG_INFO(system_log) << "info message for system";
    CIM_LOG_ERROR(system_log) << "error message for system";
    CIM_LOG_INFO(root_log) << "info message for root";

    // 测试日志管理器
    auto logger_manager = CIM::LoggerMgr::GetInstance();
    assert(logger_manager != nullptr);

    auto system_logger = logger_manager->getLogger("system");
    auto root_logger = logger_manager->getLogger("root");
    auto default_logger = logger_manager->getLogger("nonexistent");

    assert(system_logger != nullptr);
    assert(root_logger != nullptr);
    // 修改测试逻辑：验证新创建的logger有正确的根logger
    assert(default_logger != nullptr);
    assert(default_logger->getRoot() == root_logger);

    std::cout << "日志系统基本功能测试通过" << std::endl;

    // 测试YAML配置加载
    std::string before_config = logger_manager->toYamlString();
    YAML::Node root = YAML::LoadFile("/home/szy/code/CIM/bin/config/log.yaml");
    CIM::Config::LoadFromYaml(root);
    std::string after_config = logger_manager->toYamlString();

    // 配置应该发生变化
    assert(before_config != after_config);

    std::cout << "日志系统YAML配置加载测试通过" << std::endl;

    // 测试配置后的日志输出
    CIM_LOG_DEBUG(system_log) << "debug message after config";
    CIM_LOG_INFO(system_log) << "info message after config";

    std::cout << "日志系统配置后输出测试通过" << std::endl;
}

void test_logger_creation()
{
    std::cout << "=================== 测试日志器创建 ===================" << std::endl;

    // 测试获取已存在的logger
    auto logger1 = CIM_LOG_NAME("test_logger");
    auto logger2 = CIM_LOG_NAME("test_logger");

    // 应该是同一个实例
    assert(logger1 == logger2);

    // 测试日志级别设置
    logger1->setLevel(CIM::Level::ERROR);
    assert(logger1->getLevel() == CIM::Level::ERROR);

    std::cout << "日志器创建和级别设置测试通过" << std::endl;
}

void test_log_formatter()
{
    std::cout << "=================== 测试日志格式化器 ===================" << std::endl;

    auto test_logger = CIM_LOG_NAME("formatter_test");

    // 设置自定义格式
    std::string custom_format = "%d{%Y-%m-%d %H:%M:%S}%T%t%T%N%T%l%T%m%n";
    test_logger->setFormatter(custom_format);

    // 测试日志输出
    CIM_LOG_INFO(test_logger) << "测试自定义格式";

    std::cout << "日志格式化器测试通过" << std::endl;
}

void test_log_appender()
{
    std::cout << "=================== 测试日志附加器 ===================" << std::endl;

    auto test_logger = CIM_LOG_NAME("appender_test");

    // 创建并添加文件附加器
    auto file_appender = std::make_shared<CIM::FileLogAppender>("test_log.txt");
    test_logger->addAppender(file_appender);

    // 测试日志输出到文件
    CIM_LOG_INFO(test_logger) << "测试文件附加器";

    // 创建并添加控制台附加器
    auto stdout_appender = std::make_shared<CIM::StdoutLogAppender>();
    test_logger->addAppender(stdout_appender);

    CIM_LOG_DEBUG(test_logger) << "测试多个附加器";

    // 测试删除附加器
    test_logger->delAppender(file_appender);
    CIM_LOG_WARN(test_logger) << "测试删除附加器后";

    // 清空附加器
    test_logger->clearAppender();

    std::cout << "日志附加器测试通过" << std::endl;
}

void test_log_level()
{
    std::cout << "=================== 测试日志级别控制 ===================" << std::endl;

    auto test_logger = CIM_LOG_NAME("level_test");

    // 设置日志级别为ERROR
    test_logger->setLevel(CIM::Level::ERROR);

    // DEBUG和INFO级别应该不输出（因为我们无法直接验证输出，但可以通过其他方式验证）
    assert(test_logger->getLevel() == CIM::Level::ERROR);

    // 测试不同级别的日志事件创建
    auto event_debug = std::make_shared<CIM::LogEvent>(test_logger, CIM::Level::DEBUG,
                                                         __FILE__, __LINE__, 0, 0, 0, time(0), "main");
    auto event_error = std::make_shared<CIM::LogEvent>(test_logger, CIM::Level::ERROR,
                                                         __FILE__, __LINE__, 0, 0, 0, time(0), "main");

    event_debug->getSS() << "这是一条DEBUG消息";
    event_error->getSS() << "这是一条ERROR消息";

    // 调用日志方法
    test_logger->debug(event_debug);
    test_logger->error(event_error);

    std::cout << "日志级别控制测试通过" << std::endl;
}

void test_log_event()
{
    std::cout << "=================== 测试日志事件 ===================" << std::endl;

    auto test_logger = CIM_LOG_NAME("event_test");

    // 创建日志事件
    auto event = std::make_shared<CIM::LogEvent>(test_logger, CIM::Level::INFO,
                                                   "test_file.cpp", 123, 1000, 12345, 1, time(0), "main");

    // 测试格式化功能
    event->format("这是一个格式化消息, 参数1: %d, 参数2: %s", 42, "test");

    // 测试获取事件属性
    assert(std::string(event->getFileName()) == "test_file.cpp");
    assert(event->getLine() == 123);
    assert(event->getThreadId() == 12345);
    assert(event->getCoroutineId() == 1);

    // 输出日志
    test_logger->info(event);

    std::cout << "日志事件测试通过" << std::endl;
}

void test_log_rotate()
{
    std::cout << "=================== 测试日志轮转 ===================" << std::endl;

    static auto g_logger = CIM_LOG_ROOT();

    // 加载配置文件
    YAML::Node root = YAML::LoadFile("/home/szy/code/CIM/bin/config/log.yaml");
    CIM::Config::LoadFromYaml(root);

    for (int i = 0; i < 10000; ++i)
    {
        CIM_LOG_INFO(g_logger) << "日志轮转测试";
    }
}

// 原子计数器用于线程安全测试
std::atomic<int> g_log_count(0);
std::atomic<bool> g_test_running(false);

void thread_safe_log_test_func(int thread_id, int log_count) {
    auto logger = CIM_LOG_NAME("thread_safe_test");
    for (int i = 0; i < log_count && g_test_running; ++i) {
        CIM_LOG_INFO(logger) << "Thread " << thread_id << " log message #" << i;
        g_log_count++;
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
}

void test_log_thread_safety() {
    std::cout << "=================== 测试日志线程安全性 ===================" << std::endl;
    
    const int num_threads = 8;
    const int logs_per_thread = 100;
    
    g_log_count = 0;
    g_test_running = true;
    
    std::vector<std::thread> threads;
    
    auto logger = CIM_LOG_NAME("thread_safe_test");
    logger->setLevel(CIM::Level::INFO);
    
    // 创建多个线程同时写入日志
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(thread_safe_log_test_func, i, logs_per_thread);
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    g_test_running = false;
    
    std::cout << "线程安全测试完成，总共写入日志: " << g_log_count.load() << " 条" << std::endl;
    std::cout << "日志线程安全性测试通过" << std::endl;
}

void test_config_integration() {
    std::cout << "=================== 测试日志与配置集成 ===================" << std::endl;
    
    // 获取日志管理器
    auto logger_manager = CIM::LoggerMgr::GetInstance();
    
    // 保存原始配置
    std::string before_config = logger_manager->toYamlString();
    
    // 重新加载配置
    YAML::Node root = YAML::LoadFile("/home/szy/code/CIM/bin/config/log.yaml");
    CIM::Config::LoadFromYaml(root);
    
    // 检查配置是否发生变化
    std::string after_config = logger_manager->toYamlString();
    assert(before_config != after_config);
    
    // 测试重新配置后的日志输出
    auto system_logger = CIM_LOG_NAME("system");
    CIM_LOG_INFO(system_logger) << "配置集成测试消息";
    
    std::cout << "日志与配置集成测试通过" << std::endl;
}

int main(int argc, char **argv)
{
    std::cout << "开始执行日志模块全面测试" << std::endl;

    test_log_system();
    test_logger_creation();
    test_log_formatter();
    test_log_appender();
    test_log_level();
    test_log_event();
    test_log_rotate();
    test_log_thread_safety();
    test_config_integration();

    std::cout << "=================== 日志模块所有测试通过 ===================" << std::endl;
    return 0;
}