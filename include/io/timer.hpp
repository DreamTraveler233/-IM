/**
 * @file timer.hpp
 * @brief 定时器模块
 * @author your name
 * @date 2025-10-24
 * 
 * 该模块实现了基于时间事件的定时器功能，支持一次性定时器和周期性定时器。
 * 定时器使用最小堆(std::set)进行管理，通过回调函数的方式处理超时事件。
 * Timer类表示单个定时器实例，TimerManager类负责管理多个定时器。
 */

#pragma once

#include <functional>
#include <memory>
#include <set>
#include <vector>

#include "lock.hpp"

namespace CIM {
class TimerManager;

/**
     * @brief 定时器类
     * 
     * 该类表示一个定时器实例，支持一次性执行和周期性执行两种模式。
     * 每个定时器都有一个超时时间点和一个回调函数，在超时时间点到达时执行回调函数。
     */
class Timer : public std::enable_shared_from_this<Timer>, public Noncopyable {
    friend class TimerManager;

   public:
    /// 智能指针类型定义
    typedef std::shared_ptr<Timer> ptr;

    /**
         * @brief 取消定时器
         * 
         * 取消当前定时器，从定时器管理器中移除，并清空回调函数。
         * 
         * @return bool 取消成功返回true，失败返回false
         */
    bool cancel();

    /**
         * @brief 刷新定时器
         * 
         * 重新设置定时器的下次执行时间，将其从定时器管理器中移除并重新插入，
         * 新的执行时间为当前时间加上初始设定的时间间隔。
         * 
         * @return bool 刷新成功返回true，失败返回false
         */
    bool refresh();

    /**
         * @brief 重置定时器时间
         * 
         * 重置定时器的时间间隔和下次执行时间。
         * 
         * @param ms 定时器执行间隔时间(毫秒)
         * @param from_now 是否从当前时间开始计算下次执行时间
         * @return bool 重置成功返回true，失败返回false
         */
    bool reset(uint64_t ms, bool from_now);

   private:
    /**
         * @brief 构造函数
         * 
         * 创建一个定时器实例。
         * 
         * @param ms 定时器执行间隔时间(毫秒)
         * @param cb 定时器回调函数
         * @param recurring 是否周期性执行
         * @param manager 定时器管理器
         */
    Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager* manager);

    /**
         * @brief 构造函数
         * 
         * 创建一个只包含下次执行时间的定时器实例，主要用于比较操作。
         * 
         * @param next 下次执行时间点(毫秒)
         */
    Timer(uint64_t next);

   private:
    /// 是否周期性执行
    bool m_recurring = false;

    /// 定时器执行间隔时间(毫秒)
    uint64_t m_ms = 0;

    /// 下次执行时间点(毫秒)
    uint64_t m_next = 0;

    /// 定时器回调函数
    std::function<void()> m_cb;

    /// 定时器管理器指针
    TimerManager* m_manager = nullptr;

   private:
    /**
         * @brief 定时器比较仿函数
         * 
         * 用于在std::set中对定时器进行排序，按照下次执行时间排序。
         */
    struct Comparator {
        /**
             * @brief 比较两个定时器的大小
             * 
             * 按照下次执行时间进行比较，如果执行时间相同则按照对象地址比较。
             * 
             * @param lhs 左侧定时器智能指针
             * @param rhs 右侧定时器智能指针
             * @return bool 左侧小于右侧返回true，否则返回false
             */
        bool operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const;
    };
};

/**
     * @brief 定时器管理器
     * 
     * 管理多个定时器，使用std::set作为容器维护定时器的有序性。
     * 提供添加定时器、获取超时定时器回调等接口。
     */
class TimerManager : public Noncopyable {
    friend class Timer;

   public:
    /// 读写锁类型定义
    using RWMutexType = RWMutex;

    /**
         * @brief 构造函数
         */
    TimerManager();

    /**
         * @brief 析构函数
         */
    virtual ~TimerManager() = default;

    /**
         * @brief 添加定时器
         * 
         * 添加一个定时器到管理器中。
         * 
         * @param ms 定时器执行间隔时间(毫秒)
         * @param cb 定时器回调函数
         * @param recurring 是否周期性执行，默认为false
         * @return Timer::ptr 创建的定时器智能指针
         */
    Timer::ptr addTimer(uint64_t ms, std::function<void()> cb, bool recurring = false);

    /**
         * @brief 添加条件定时器
         * 
         * 添加一个带条件判断的定时器，只有当weak_cond有效时才会执行回调函数。
         * 
         * @param ms 定时器执行间隔时间(毫秒)
         * @param cb 定时器回调函数
         * @param weak_cond 条件弱指针
         * @param recurring 是否周期性执行，默认为false
         * @return Timer::ptr 创建的定时器智能指针
         */
    Timer::ptr addConditionTimer(uint64_t ms, std::function<void()> cb,
                                 std::weak_ptr<void> weak_cond, bool recurring = false);

    /**
         * @brief 获取最近的定时器执行时间
         * 
         * 获取距离当前时间最近的定时器执行时间间隔。
         * 
         * @return uint64_t 距离最近定时器执行的时间(毫秒)，如果无定时器则返回~0ull
         */
    uint64_t getNextTimer();

    /**
         * @brief 获取并处理超时的定时器回调
         * 
         * 获取所有超时的定时器的回调函数，并处理周期性执行的定时器。
         * 
         * @param cbs 回调函数容器，用于返回超时的定时器回调函数
         */
    void listExpiredCb(std::vector<std::function<void()>>& cbs);

    /**
         * @brief 检查是否存在定时器
         * 
         * @return bool 存在定时器返回true，否则返回false
         */
    bool hasTimer();

   protected:
    /**
         * @brief 当有定时器被插入到队列头部时调用
         * 
         * 纯虚函数，由子类实现具体逻辑。
         */
    virtual void onTimerInsertedAtFront() = 0;

    /**
         * @brief 添加定时器到管理器
         * 
         * @param val 定时器智能指针
         * @param lock 写锁对象
         */
    void addTimer(Timer::ptr val, RWMutexType::WriteLock& lock);

   private:
    /**
         * @brief 检测系统时钟是否回退
         * 
         * 检测系统时钟是否发生了回退（用户修改了系统时间等情况）。
         * 
         * @param now_ms 当前时间戳(毫秒)
         * @return bool 发生回退返回true，否则返回false
         */
    bool detectClockRollover(uint64_t now_ms);

   private:
    /// 读写互斥锁
    RWMutexType m_mutex;

    /// 定时器集合，按执行时间排序
    std::set<Timer::ptr, Timer::Comparator> m_timers;

    /// 是否被"踢"过，用于避免频繁触发onTimerInsertedAtFront
    bool m_tickled = false;

    /// 上次检查时间，用于检测系统时钟回退
    uint64_t m_previouseTime = 0;
};
}  // namespace CIM