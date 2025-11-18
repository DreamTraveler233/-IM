/**
 * @file coroutine.hpp
 * @brief 协程实现模块
 *
 * 该文件提供了协程的实现，包括协程的创建、切换、状态管理等功能。
 * 基于ucontext实现协程上下文切换，支持协程的挂起、恢复等操作。
 */

#pragma once

#include <ucontext.h>

#include <functional>
#include <memory>

#include "base/noncopyable.hpp"

namespace CIM {
/**
     * @brief 协程类
     *
     * 实现了协程的基本功能，包括创建、执行、挂起、恢复等操作。
     * 使用ucontext_t保存和恢复协程上下文，通过状态机管理协程生命周期。
     */
class Coroutine : public std::enable_shared_from_this<Coroutine>, Noncopyable {
   public:
    /// 智能指针类型定义
    using ptr = std::shared_ptr<Coroutine>;

    /**
         * @brief 协程状态枚举
         */
    enum State {
        INIT,   /// 初始化状态 - 表示协程刚创建或者是重置后，等待调度
        HOLD,   /// 暂停状态 - 表示协程被挂起，等待外部条件满足，继续执行
        EXEC,   /// 执行状态 - 表示协程正在执行中，当前拥有线程执行权
        TERM,   /// 终止状态 - 表示协程已执行完毕，生命周期结束
        READY,  /// 就绪状态 - 表示协程主动让出执行权，但希望继续执行
        EXCEPT  /// 异常状态 - 表示协程执行过程中发生异常，已终止
    };

   private:
    /**
         * @brief 默认构造函数，用于创建主协程
         */
    Coroutine();

   public:
    /**
         * @brief 构造函数
         * @param[in] cb 协程执行的回调函数
         * @param[in] stack_size 协程栈大小，默认为0表示使用默认大小
         * @param[in] use_caller 是否使用调用者上下文，默认为false
         */
    Coroutine(std::function<void()> cb, size_t stack_size = 0, bool use_caller = false);

    /**
         * @brief 析构函数
         */
    ~Coroutine();

    /**
         * @brief 重置协程函数，并重置状态
         * @param[in] cb 新的协程执行回调函数
         */
    void reset(std::function<void()> cb);

    /**
         * @brief 切换到当前协程执行
         */
    void swapIn();

    /**
         * @brief 协程让出执行权
         */
    void swapOut();

    /**
         * @brief 调用当前协程
         */
    void call();

    /**
         * @brief 从当前协程返回
         */
    void back();

    /**
         * @brief 获取协程ID
         * @return 协程ID
         */
    uint64_t getId() const;

    /**
         * @brief 获取协程状态
         * @return 协程状态
         */
    State getState() const;

    /**
         * @brief 设置协程状态
         * @param[in] state 协程状态
         */
    void setState(State state);

   public:
    /**
         * @brief 设置当前协程
         * @param[in] f 协程对象指针
         */
    static void SetThis(Coroutine* f);

    /**
         * @brief 获取当前协程
         * @return 当前协程智能指针
         */
    static Coroutine::ptr GetThis();

    /**
         * @brief 协程切换到就绪状态
         */
    static void YieldToReady();

    /**
         * @brief 协程切换到挂起状态
         */
    static void YieldToHold();

    /**
         * @brief 获取协程总数
         * @return 协程总数
         */
    static uint64_t TotalCoroutines();

    /**
         * @brief 协程主函数
         */
    static void MainFunc();

    /**
         * @brief 调用者主函数
         */
    static void CallerMainFunc();

    /**
         * @brief 获取当前协程ID
         * @return 当前协程ID
         */
    static uint64_t GetCoroutineId();

   private:
    uint64_t m_id = 0;            /// 协程id
    uint32_t m_stack_size = 0;    /// 协程栈大小
    State m_state = State::INIT;  /// 协程当前状态
    ucontext_t m_ctx;             /// 协程上下文，用于保存和切换上下文环境
    void* m_stack = nullptr;      /// 协程栈空间
    std::function<void()> m_cb;   /// 协程要执行的回调函数
};

/**
     * @brief 协程栈分配器
     *
     * 提供协程栈空间的分配和释放功能。
     */
class MallocStackAllocator : public Noncopyable {
   public:
    /**
         * @brief 分配栈空间
         * @param[in] size 栈大小
         * @return 分配的栈空间指针
         */
    static void* Alloc(size_t size);

    /**
         * @brief 释放栈空间
         * @param[in] ptr 栈空间指针
         * @param[in] size 栈大小
         */
    static void Dealloc(void* ptr, size_t size);
};
}  // namespace CIM