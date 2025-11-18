#include "base/macro.hpp"
#include "logger.hpp"
#include "log_appender.hpp"
#include <chrono>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <iomanip>
#include <map>

// 性能测试结果结构体
struct PerformanceResult
{
    int thread_count;         // 线程数量
    int log_count_per_thread; // 每个线程写入的日志条数
    long long total_logs;     // 总日志条数
    long long duration_ms;    // 执行时间（毫秒）
    double logs_per_second;   // 每秒日志条数（吞吐量）
    double avg_latency_us;    // 平均延迟（微秒/每条日志）
};

// 全局变量用于统计
std::atomic<long long> g_total_log_count{0};   // 日志总数
std::atomic<long long> g_total_duration_us{0}; // 累计总耗时（微秒)
long long g_overall_log_count = 0;             // 整体日志计数
long long g_overall_duration_us = 0;           // 整体耗时

// 存储所有测试结果的容器
std::map<std::string, PerformanceResult> g_test_results;

// 测试不同日志级别的写入性能
void testLogLevelPerformance(CIM::Logger::ptr logger, CIM::Level level,
                             int log_count, PerformanceResult &result)
{
    auto start_time = std::chrono::high_resolution_clock::now();

    switch (level)
    {
    case CIM::Level::DEBUG:
        for (int i = 0; i < log_count; ++i)
        {
            CIM_LOG_DEBUG(logger) << "Debug message " << i << " for performance test";
        }
        break;
    case CIM::Level::INFO:
        for (int i = 0; i < log_count; ++i)
        {
            CIM_LOG_INFO(logger) << "Info message " << i << " for performance test";
        }
        break;
    case CIM::Level::WARN:
        for (int i = 0; i < log_count; ++i)
        {
            CIM_LOG_WARN(logger) << "Warn message " << i << " for performance test";
        }
        break;
    case CIM::Level::ERROR:
        for (int i = 0; i < log_count; ++i)
        {
            CIM_LOG_ERROR(logger) << "Error message " << i << " for performance test";
        }
        break;
    case CIM::Level::FATAL:
        for (int i = 0; i < log_count; ++i)
        {
            CIM_LOG_FATAL(logger) << "Fatal message " << i << " for performance test";
        }
        break;
    case CIM::Level::UNKNOWN:
    default:
        for (int i = 0; i < log_count; ++i)
        {
            CIM_LOG_INFO(logger) << "Unknown level message " << i << " for performance test";
        }
        break;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    result.thread_count = 1;
    result.log_count_per_thread = log_count;
    result.total_logs = log_count;
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    result.logs_per_second = (result.duration_ms > 0) ? (log_count * 1000.0 / result.duration_ms) : 0;
    result.avg_latency_us = (log_count > 0) ? (duration.count() / (double)log_count) : 0;

    g_total_log_count += log_count;
    g_total_duration_us += duration.count();
}

// 测试格式化日志的性能
void testFormattedLogPerformance(CIM::Logger::ptr logger, int log_count, PerformanceResult &result)
{
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < log_count; ++i)
    {
        CIM_LOG_FMT_INFO(logger, "Formatted log message %d with value %f and string %s",
                           i, 3.14159, "test");
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    result.thread_count = 1;
    result.log_count_per_thread = log_count;
    result.total_logs = log_count;
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    result.logs_per_second = (result.duration_ms > 0) ? (log_count * 1000.0 / result.duration_ms) : 0;
    result.avg_latency_us = (log_count > 0) ? (duration.count() / (double)log_count) : 0;

    g_total_log_count += log_count;
    g_total_duration_us += duration.count();
}

// 多线程性能测试函数
void multiThreadPerformanceTest(CIM::Logger::ptr logger, int log_count)
{
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < log_count; ++i)
    {
        CIM_LOG_INFO(logger) << "Multithread log message " << i << " from thread "
                               << std::this_thread::get_id();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    g_total_log_count += log_count;
    g_total_duration_us += duration.count();
}

// 打印性能测试结果
void printPerformanceResult(const std::string &test_name, const PerformanceResult &result)
{
    // 保存结果供最后汇总显示
    g_test_results[test_name] = result;
}

// 打印所有测试结果汇总
void printAllTestResults()
{
    std::cout << "\n==================== 测试结果汇总 ====================" << std::endl;
    for (const auto &pair : g_test_results)
    {
        const std::string &test_name = pair.first;
        const PerformanceResult &result = pair.second;

        std::cout << "\n--- " << test_name << " ---" << std::endl;
        std::cout << "  线程数: " << result.thread_count << std::endl;
        std::cout << "  每线程日志数: " << result.log_count_per_thread << std::endl;
        std::cout << "  总日志数: " << result.total_logs << std::endl;
        std::cout << "  总耗时: " << result.duration_ms << " ms" << std::endl;
        std::cout << "  吞吐量: " << std::fixed << std::setprecision(0) << result.logs_per_second << " logs/sec" << std::endl;
        std::cout << "  平均延迟: " << std::fixed << std::setprecision(2) << result.avg_latency_us << " μs/log" << std::endl;
    }

    // 总结
    std::cout << "\n总体性能总结:\n"
              << std::endl;
    std::cout << "在整个测试过程中总共写入了 " << g_overall_log_count << " 条日志" << std::endl;
    std::cout << "总耗时: " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::microseconds(g_overall_duration_us)).count() << " ms" << std::endl;

    if (g_overall_duration_us > 0)
    {
        double overall_throughput = g_overall_log_count * 1000000.0 / g_overall_duration_us;
        std::cout << "总体平均吞吐量: " << std::fixed << std::setprecision(0) << overall_throughput << " logs/sec" << std::endl;
    }
    std::cout << "=====================================================" << std::endl;
}

int main(int argc, char **argv)
{
    std::cout << "日志系统写入效率测试" << std::endl;
    std::cout << "========================" << std::endl;

    // 创建测试logger
    auto logger = CIM_LOG_ROOT();
    logger->setLevel(CIM::Level::DEBUG);

    // 添加文件appender用于测试
    auto file_appender = std::make_shared<CIM::FileLogAppender>("./log/performance_test.log");
    logger->addAppender(file_appender);

    // 添加控制台appender
    // auto console_appender = std::make_shared<CIM::StdoutLogAppender>();
    // logger->addAppender(console_appender);

    PerformanceResult result;

    // 测试不同日志级别的性能
    std::cout << "\n1. 测试不同日志级别的写入性能 (单线程, 10000条日志):\n"
              << std::endl;

    g_total_log_count = 0;
    g_total_duration_us = 0;

    testLogLevelPerformance(logger, CIM::Level::DEBUG, 10000, result);
    printPerformanceResult("DEBUG级别性能测试", result);

    testLogLevelPerformance(logger, CIM::Level::INFO, 10000, result);
    printPerformanceResult("INFO级别性能测试", result);

    testLogLevelPerformance(logger, CIM::Level::WARN, 10000, result);
    printPerformanceResult("WARN级别性能测试", result);

    testLogLevelPerformance(logger, CIM::Level::ERROR, 10000, result);
    printPerformanceResult("ERROR级别性能测试", result);

    testLogLevelPerformance(logger, CIM::Level::FATAL, 10000, result);
    printPerformanceResult("FATAL级别性能测试", result);

    g_overall_log_count += 50000;                        // 累计日志数
    g_overall_duration_us += g_total_duration_us.load(); // 累计耗时

    // 测试格式化日志性能
    std::cout << "\n2. 测试格式化日志的写入性能 (单线程, 10000条日志):\n"
              << std::endl;

    testFormattedLogPerformance(logger, 10000, result);
    printPerformanceResult("格式化日志性能测试", result);

    g_overall_log_count += 10000;                        // 累计日志数
    g_overall_duration_us += g_total_duration_us.load(); // 累计耗时

    // 多线程性能测试
    std::cout << "\n3. 测试多线程并发写入性能:\n"
              << std::endl;

    g_total_log_count = 0;
    g_total_duration_us = 0;

    const int thread_count = 4;
    const int log_count_per_thread = 25000; // 每线程25000条日志，总共10000条

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < thread_count; ++i)
    {
        threads.emplace_back(multiThreadPerformanceTest, logger, log_count_per_thread);
    }

    for (auto &t : threads)
    {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    auto total_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(total_duration).count();

    long long total_logs = thread_count * log_count_per_thread;
    double logs_per_second = (total_duration_ms > 0) ? (total_logs * 1000.0 / total_duration_ms) : 0;
    double avg_latency_us = (total_logs > 0) ? (total_duration.count() / (double)total_logs) : 0;

    // 将多线程测试结果保存到结果集中
    PerformanceResult multithread_result;
    multithread_result.thread_count = thread_count;
    multithread_result.log_count_per_thread = log_count_per_thread;
    multithread_result.total_logs = total_logs;
    multithread_result.duration_ms = total_duration_ms;
    multithread_result.logs_per_second = logs_per_second;
    multithread_result.avg_latency_us = avg_latency_us;
    printPerformanceResult("多线程并发写入性能测试", multithread_result);

    g_overall_log_count += total_logs;               // 累计日志数
    g_overall_duration_us += total_duration.count(); // 累计耗时

    // 测试不同appender的性能
    std::cout << "\n4. 测试不同Appender的写入性能:\n"
              << std::endl;

    // 仅控制台appender
    auto logger_console = CIM_LOG_NAME("console_only");
    logger_console->addAppender(std::make_shared<CIM::StdoutLogAppender>());

    g_total_log_count = 0;
    g_total_duration_us = 0;

    testLogLevelPerformance(logger_console, CIM::Level::INFO, 10000, result);
    printPerformanceResult("仅控制台Appender性能测试", result);

    // 仅文件appender
    auto logger_file = CIM_LOG_NAME("file_only");
    logger_file->addAppender(std::make_shared<CIM::FileLogAppender>("./log/file_only_test.log"));

    testLogLevelPerformance(logger_file, CIM::Level::INFO, 10000, result);
    printPerformanceResult("仅文件Appender性能测试", result);

    // 混合appender
    auto logger_mixed = CIM_LOG_NAME("mixed");
    logger_mixed->addAppender(std::make_shared<CIM::StdoutLogAppender>());
    logger_mixed->addAppender(std::make_shared<CIM::FileLogAppender>("./log/mixed_test.log"));

    testLogLevelPerformance(logger_mixed, CIM::Level::INFO, 10000, result);
    printPerformanceResult("混合Appender性能测试", result);

    g_overall_log_count += 30000;                        // 累计日志数
    g_overall_duration_us += g_total_duration_us.load(); // 累计耗时

    // 显示所有测试结果汇总
    printAllTestResults();

    std::cout << "\n测试完成!" << std::endl;

    return 0;
}