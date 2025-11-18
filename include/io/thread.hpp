/**
 * @file thread.hpp
 * @brief 线程封装类
 * @author szy
 *
 * 该文件定义了对 POSIX 线程的面向对象封装，提供了线程创建、管理、同步等功能。
 * 通过该类可以更方便地创建和控制线程，并支持获取线程信息、设置线程名称等操作。
 */

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "base/noncopyable.hpp"
#include "semaphore.hpp"

namespace CIM {
/**
     * @brief 线程类，对pthread进行封装
     *
     * 提供了线程创建、管理、同步等功能，支持设置线程回调函数和线程名称。
     * 线程在创建时自动启动，并通过信号量机制确保线程初始化完成后再返回。
     */
class Thread : public Noncopyable {
   public:
    /// 智能指针类型定义
    using ptr = std::shared_ptr<Thread>;

    /**
         * @brief 构造函数
         * @param[in] cb 线程执行的回调函数
         * @param[in] name 线程名称
         *
         * 创建并启动线程，线程函数为run，参数为this
         */
    Thread(std::function<void()> cb, const std::string& name = "UNKNOWN");

    /**
         * @brief 析构函数
         *
         * 等待线程结束后清理资源
         */
    ~Thread();

    /**
         * @brief 获取线程ID
         * @return 线程ID
         */
    pid_t getId() const;

    /**
         * @brief 获取线程名称
         * @return 线程名称
         */
    const std::string& getName() const;

    /**
         * @brief 等待线程结束
         *
         * 调用pthread_join等待线程执行完毕
         */
    void join();

    /**
         * @brief 获取当前线程对象
         * @return 当前线程对象指针
         */
    static Thread* GetThis();

    /**
         * @brief 获取当前线程名称
         * @return 当前线程名称
         */
    static const std::string& GetName();

    /**
         * @brief 设置当前线程名称
         * @param[in] name 线程名称
         */
    static void SetName(const std::string& name);

   private:
    /**
         * @brief 线程执行函数
         * @param[in] arg 线程参数，实际为Thread对象指针
         * @return 线程返回值
         *
         * 内部调用m_cb回调函数，并处理线程相关的初始化工作
         */
    static void* run(void* arg);

   private:
    pid_t m_id;                  // 线程ID
    pthread_t m_thread;          // 线程句柄
    std::function<void()> m_cb;  // 线程回调函数
    std::string m_name;          // 线程名称
    Semaphore m_semaphore;       // 信号量，用于线程启动同步
};
}  // namespace CIM