/**
 * @file scheduler.hpp
 * @brief 协程调度器实现
 * @author CIM
 *
 * 该文件定义了协程调度器Scheduler和调度切换器SchedulerSwitcher类。
 * Scheduler负责管理线程池和协程队列，实现协程的调度执行；
 * SchedulerSwitcher用于临时切换到指定的调度器上下文执行。
 */

#pragma once

#include <list>
#include <vector>

#include "coroutine.hpp"
#include "lock.hpp"
#include "base/noncopyable.hpp"
#include "thread.hpp"

/*
==============================协程调度器模型==============================

线程层面:
+---------------------+     +---------------------+     +---------------------+
|     调用线程        |     |     工作线程1        |     |     工作线程2        |
|    协程调度器       |     |     协程调度器       |     |     协程调度器       |
|  (use_caller=true)  |     |                     |     |                     |
+----------+---------+     +----------+----------+     +----------+----------+
           |                           |                           |
           | 共享任务队列               | 共享任务队列               | 共享任务队列
           |                           |                           |
协程层面:   |                           |                           |
           |                           |                           |
+----------v----------+     +----------v----------+     +----------v----------+
|   线程主协程        |     |   线程主协程        |     |   线程主协程        |
|  (Coroutine::GetThis)|     | (Coroutine::GetThis)|     | (Coroutine::GetThis)|
|                     |     |                     |     |                     |
| +-----------------+ |     | +-----------------+ |     | +-----------------+ |
| | 调度器根协程    | |     | | 任务协程A       | |     | | 任务协程B       | |
| | (m_rootCoroutine)| |     | | (从队列获取)    | |     | | (从队列获取)    | |
| | 绑定到run方法   | |     | |                 | |     | |                 | |
| +-----------------+ |     | +-----------------+ |     | +-----------------+ |
|                     |     |                     |     |                     |
| +-----------------+ |     | +-----------------+ |     | +-----------------+ |
| | 空闲协程(idle)  | |     | | 空闲协程(idle)  | |     | | 空闲协程(idle)  | |
| | 处理空闲状态    | |     | | 处理空闲状态    | |     | | 处理空闲状态    | |
| +-----------------+ |     | +-----------------+ |     | +-----------------+ |
|                     |     |                     |     |                     |
| +-----------------+ |     | +-----------------+ |     | +-----------------+ |
| | 回调协程        | |     | | 回调协程        | |     | | 回调协程        | |
| | (cb_coroutine)  | |     | | (cb_coroutine)  | |     | | (cb_coroutine)  | |
| | 执行函数回调    | |     | | 执行函数回调    | |     | | 执行函数回调    | |
| +-----------------+ |     | +-----------------+ |     | +-----------------+ |
+---------------------+     +---------------------+     +---------------------+
           ^                           ^                           ^
           |                           |                           |
           +------------+--------------+--------------+------------+
                        |                             |
                        v                             v
              +------------------+       +------------------+
              |  任务队列        |<----->|  线程同步机制    |
              |  m_taskQueue     |       |  (互斥锁等)      |
              | (线程安全共享)   |       +------------------+
              +------------------+

协程类型说明：

1. 线程主协程 (Thread Main Coroutine):
   - 每个线程都有一个，通过 Coroutine::GetThis() 获取
   - 作为协程切换的基础上下文
   - 是所有协程切换的"返回目标"

2. 调度器根协程 (Scheduler Root Coroutine):
   - 仅在 use_caller=true 时存在于调用线程中
   - 通过 m_rootCoroutine 引用
   - 绑定到 Scheduler::run 方法，使调用线程成为工作线程

3. 任务协程 (Task Coroutine):
   - 通过 schedule 方法加入任务队列
   - 由各工作线程竞争获取并执行
   - 执行完成后根据状态决定后续处理

4. 空闲协程 (Idle Coroutine):
   - 当没有任务可执行时运行
   - 通过 yield 让出执行权，等待新任务
   - 保持线程活跃状态

5. 回调协程 (Callback Coroutine):
   - 用于执行 std::function 回调函数
   - 可复用以减少协程创建开销
   - 执行完回调后根据状态处理后续逻辑

线程执行流程：

调用线程 (use_caller=true):
  1. 显式初始化主协程: Coroutine::GetThis()
  2. 创建调度器根协程: m_rootCoroutine (绑定到 run 方法)
  3. 通过 m_rootCoroutine 执行 run 方法，成为工作线程

工作线程:
  1. 线程开始执行时隐式创建主协程
  2. 在 run 方法中通过 Coroutine::GetThis() 获取主协程
  3. 执行任务调度逻辑

统一的调度模型:
  所有线程(包括调用线程和工作线程)都执行相同的 run 方法，
  从共享任务队列中竞争获取任务执行，实现了统一的工作线程模型。
 */

namespace CIM {
/**
     * @brief 协程调度器类
     * @details 负责协程的调度，维护线程池和协程队列，实现协程在不同线程间的分发执行
     */
class Scheduler : public Noncopyable {
   public:
    /// 智能指针类型定义
    using ptr = std::shared_ptr<Scheduler>;
    /// 互斥锁类型定义
    using MutexType = Mutex;

    /**
         * @brief 构造函数
         * @param[in] threads 线程数量，默认为1
         * @param[in] use_caller 是否使用调用线程作为调度线程，默认为true
         * @param[in] name 调度器名称，默认为空
         */
    Scheduler(size_t threads = 1, bool use_caller = true, const std::string& name = "");

    /**
         * @brief 析构函数
         */
    virtual ~Scheduler();

    /**
         * @brief 获取调度器名称
         * @return const std::string& 调度器名称
         */
    const std::string& getName() const;

    /**
         * @brief 获取当前线程的调度器实例
         * @return Scheduler* 当前线程的调度器实例
         */
    static Scheduler* GetThis();

    /**
         * @brief 获取当前线程的主协程
         * @return Coroutine* 当前线程的主协程
         */
    static Coroutine* GetMainCoroutine();

    /**
         * @brief 启动调度器
         */
    void start();

    /**
         * @brief 停止调度器
         */
    void stop();

    /**
         * @brief 用于调度单个协程或回调函数
         *
         * @param cb 要调度的协程或回调函数
         * @param tid 指定执行该任务的线程ID，默认为-1表示任意线程都可以执行
         */
    template <class CoroutineOrcb>
    void schedule(CoroutineOrcb cb, uint64_t tid = -1) {
        bool need_tickle = false;  // 用于标记是否需要唤醒工作线程
        {
            MutexType::Lock lock(m_mutex);
            need_tickle = scheduleNolock(cb, tid);
        }
        if (need_tickle) {
            tickle();  // 唤醒工作线程
        }
    }

    /**
         * @brief 用于批量调度协程或回调函数
         *
         * @param begin 任务列表的起始迭代器
         * @param end 任务列表的结束迭代器
         */
    template <class InputIterator>
    void schedule(InputIterator begin, InputIterator end) {
        bool need_tickle = false;  // 用于标记是否需要唤醒工作线程
        {
            MutexType::Lock lock(m_mutex);
            while (begin != end) {
                // 将每个任务通过scheduleNolock添加到调度队列
                need_tickle = scheduleNolock(&*begin, -1) || need_tickle;
                ++begin;
            }
        }
        if (need_tickle) {
            tickle();  // 唤醒其他线程
        }
    }

    /**
         * @brief 切换到指定线程执行
         * @param thread 指定的线程ID，默认为-1表示任意线程
         */
    void switchTo(int thread = -1);

    /**
         * @brief 输出调度器信息到输出流
         * @param os 输出流
         * @return std::ostream& 输出流
         */
    std::ostream& dump(std::ostream& os);

   protected:
    /**
         * @brief 唤醒空闲线程
         */
    virtual void tickle();

    /**
         * @brief 判断调度器是否应该停止
         * @return bool true表示应该停止
         */
    virtual bool stopping();

    /**
         * @brief 空闲时执行的函数
         */
    virtual void idle();

    /**
         * @brief 线程运行函数
         */
    void run();

    /**
         * @brief 设置当前线程的调度器
         */
    void setThis();

    /**
         * @brief 判断是否有空闲线程
         * @return bool true表示有空闲线程
         */
    bool hasIdleThreads();

   private:
    /**
         * @brief 用于在不加锁的情况下将协程或回调函数添加到调度队列中
         *
         * @param cb 要调度的协程或回调函数
         * @param tid 指定执行该任务的线程ID，默认为-1表示任意线程都可以执行
         * @return true 需要唤醒工作线程
         * @return false 不需要唤醒工作线程
         */
    template <class CoroutineOrCb>
    bool scheduleNolock(CoroutineOrCb cb, uint64_t tid) {
        // 如果队列不为空，说明有其他任务正在等待处理，工作线程应该已经在运行或即将运行
        // 如果队列为空，工作线程可能处于空闲状态，需要主动唤醒以处理新任务
        bool need_tickle = m_taskQueue.empty();
        Task task(cb, tid);
        if (task.coroutine || task.cb) {
            m_taskQueue.push_back(task);
        }
        return need_tickle;
    }

   private:
    /**
         * @brief 协程和线程的封装结构体
         * @details 用于封装待执行的协程或回调函数及其指定执行线程的信息
         */
    struct Task {
        Coroutine::ptr coroutine;  ///< 协程智能指针，存储待执行的协程对象
        std::function<void()> cb;  ///< 回调函数，存储待执行的普通函数对象
        pid_t threadId;            ///< 线程ID，指定该任务应在哪个线程上执行，-1表示任意线程

        /**
             * @brief 构造函数，使用协程指针和线程ID初始化
             * @param[in] c 协程智能指针
             * @param[in] tid 线程ID
             */
        Task(Coroutine::ptr c, uint64_t tid) : coroutine(c), threadId(tid) {}

        /**
             * @brief 构造函数，使用协程指针引用和线程ID初始化
             * @param[in] c 协程智能指针引用
             * @param[in] tid 线程ID
             */
        Task(Coroutine::ptr* c, uint64_t tid) : threadId(tid) { coroutine.swap(*c); }

        /**
             * @brief 构造函数，使用回调函数和线程ID初始化
             * @param[in] f 回调函数
             * @param[in] tid 线程ID
             */
        Task(std::function<void()> f, uint64_t tid) : cb(f), threadId(tid) {}

        /**
             * @brief 构造函数，使用回调函数引用和线程ID初始化
             * @param[in] f 回调函数引用
             * @param[in] tid 线程ID
             */
        Task(std::function<void()>* f, uint64_t tid) : threadId(tid) { cb.swap(*f); }

        /**
             * @brief 默认构造函数
             */
        Task() : threadId(-1) {}

        /**
             * @brief 重置所有成员变量
             */
        void reset() {
            coroutine = nullptr;
            cb = nullptr;
            threadId = -1;
        }
    };

   private:
    MutexType m_mutex;                   ///< 互斥锁，保护协程队列和线程安全
    std::vector<Thread::ptr> m_threads;  ///< 线程池，存储所有工作线程
    std::list<Task> m_taskQueue;         ///< 待执行的协程队列，存储待调度的协程和回调函数
    Coroutine::ptr m_rootCoroutine;      ///< 主协程，调度器的根协程，负责调度其他协程
    std::string m_name;                  ///< 协程调度器的名称

   protected:
    std::vector<pid_t> m_threadIds;                 ///< 线程ID列表，存储工作线程的ID
    size_t m_threadCount = 0;                       ///< 工作线程数量
    std::atomic<size_t> m_activeThreadCount = {0};  ///< 活跃线程数量（正在执行协程的线程数）
    std::atomic<size_t> m_idleThreadCount = {0};    ///< 空闲线程数量（等待任务的线程数）
    bool m_isRunning = false;                       ///< 调度器是否在运行
    bool m_autoStop = false;                        ///< 是否自动停止（当没有任务时自动停止）
    pid_t m_rootThreadId = 0;                       ///< 主线程ID（使用调用线程时的线程ID）
};

/**
     * @brief 调度器切换器类
     * @details 用于临时切换到指定的调度器上下文执行，析构时自动切换回原调度器
     */
class SchedulerSwitcher : public Noncopyable {
   public:
    /**
         * @brief 构造函数，切换到目标调度器
         * @param[in] target 目标调度器，默认为nullptr
         */
    SchedulerSwitcher(Scheduler* target = nullptr);

    /**
         * @brief 析构函数，自动切换回原调度器
         */
    ~SchedulerSwitcher();

   private:
    Scheduler* m_caller;  ///< 调用者调度器，用于保存原调度器上下文
};
}  // namespace CIM