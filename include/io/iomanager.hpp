/**
 * @file iomanager.hpp
 * @brief 基于epoll的IO事件管理器实现
 * @author CIM
 *
 * 该文件定义了IOManager类，它是一个基于epoll的异步IO事件管理器，
 * 继承自Scheduler（协程调度器）和TimerManager（定时器管理器）。
 * IOManager能够监听文件描述符上的读写事件，并在事件就绪时自动调度
 * 相应的协程或回调函数执行，从而实现高效的异步IO编程。
 */

#pragma once

#include "scheduler.hpp"
#include "timer.hpp"

namespace CIM {
/**
     * @brief IO事件管理器
     * @details IOManager是一个基于epoll的I/O多路复用事件管理器，继承自Scheduler和TimerManager。
     *          它能够监听文件描述符上的读写事件，并在事件触发时自动调度相应的协程或回调函数执行。
     *          同时，它也具备定时器管理功能，可以调度定时任务。
     */
class IOManager : public Scheduler, public TimerManager {
   public:
    using ptr = std::shared_ptr<IOManager>;
    using RWMutexType = RWMutex;

    /**
         * @brief IO事件类型枚举
         * @details 定义了可以被IOManager监听和处理的事件类型
         */
    enum Event {
        NONE = 0x0,   /// 无事件
        READ = 0x1,   /// 读事件(EPOLLIN)
        WRITE = 0x4,  /// 写事件(EPOLLOUT)
    };

   private:
    /**
         * @brief 文件描述符上下文结构体
         * @details 管理特定文件描述符上的事件信息，包括读写事件上下文和当前监听的事件类型
         */
    struct FdContext {
        using MutexType = Mutex;

        /**
             * @brief 事件上下文结构体
             * @details 存储特定事件（读或写）的相关信息，包括关联的调度器、协程和回调函数
             */
        struct EventContext {
            Scheduler* scheduler = nullptr;  /// 指定执行事件的调度器
            Coroutine::ptr coroutine;        /// 绑定到事件的协程对象
            std::function<void()> cb;        /// 事件触发时执行的回调函数
        };

        /**
             * @brief 获取指定事件类型的上下文对象
             * @param[in] event 事件类型（READ或WRITE）
             * @return EventContext& 对应事件的上下文引用
             */
        EventContext& getContext(Event event);

        /**
             * @brief 重置指定事件上下文的状态
             * @param[in] event 需要重置的事件上下文
             */
        void resetContext(EventContext& event);

        /**
             * @brief 触发指定事件并调度相应的协程或回调函数
             * @param[in] event 需要触发的事件类型
             */
        void triggerEvent(Event event);

        int fd = 0;           /// 事件绑定的文件描述符
        EventContext read;    /// 读事件上下文
        EventContext write;   /// 写事件上下文
        Event events = NONE;  /// 当前监听的事件类型
        MutexType mutex;      /// 保护该上下文的互斥锁
    };

   public:
    /**
         * @brief 构造函数
         * @param[in] threads 线程数量
         * @param[in] use_caller 是否使用调用线程作为调度线程之一
         * @param[in] name 调度器名称
         */
    IOManager(size_t threads = 1, bool use_caller = true, const std::string& name = "");

    /**
         * @brief 析构函数
         */
    ~IOManager();

    /**
         * @brief 添加事件监听
         * @param[in] fd 文件描述符
         * @param[in] event 事件类型
         * @param[in] cb 事件触发时执行的回调函数，如果为空则使用当前协程
         * @return bool 添加成功返回true，失败返回false
         */
    bool addEvent(int fd, Event event, std::function<void()> cb = nullptr);

    /**
         * @brief 删除事件监听
         * @param[in] fd 文件描述符
         * @param[in] event 事件类型
         * @return bool 删除成功返回true，失败返回false
         */
    bool delEvent(int fd, Event event);

    /**
         * @brief 取消事件监听并触发事件处理
         * @param[in] fd 文件描述符
         * @param[in] event 事件类型
         * @return bool 取消成功返回true，失败返回false
         */
    bool cancelEvent(int fd, Event event);

    /**
         * @brief 取消指定文件描述符上的所有事件监听
         * @param[in] fd 文件描述符
         * @return bool 取消成功返回true，失败返回false
         */
    bool cancelAll(int fd);

    /**
         * @brief 获取当前线程的IOManager实例
         * @return IOManager* 当前线程的IOManager实例指针
         */
    static IOManager* GetThis();

   protected:
    /**
         * @brief 唤醒空闲线程
         */
    void tickle() override;

    /**
         * @brief 判断调度器是否应该停止
         * @return bool true表示应该停止
         */
    bool stopping() override;

    /**
         * @brief 空闲时执行的函数，处理I/O事件
         */
    void idle() override;

    /**
         * @brief 当定时器被插入到队列前端时的处理函数
         */
    void onTimerInsertedAtFront() override;

    /**
         * @brief 调整文件描述符上下文数组大小
         * @param[in] size 新的数组大小
         */
    void contextResize(size_t size);

    /**
         * @brief 带超时时间的停止判断函数
         * @param[out] timeout 超时时间
         * @return bool true表示应该停止
         */
    bool stopping(uint64_t& timeout);

   private:
    int m_epfd = 0;                                 /// epoll文件描述符
    int m_tickleFds[2];                             /// 用于唤醒epoll_wait的管道文件描述符
    std::atomic<size_t> m_pendingEventCount = {0};  /// 待处理的事件数量
    RWMutexType m_mutex;                            /// 保护文件描述符上下文数组的读写锁
    std::vector<FdContext*> m_fdContexts;           /// 文件描述符上下文数组
};
}  // namespace CIM