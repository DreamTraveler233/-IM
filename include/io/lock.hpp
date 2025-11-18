/**
 * @file lock.hpp
 * @brief 线程同步锁封装模块
 * @author CIM
 *
 * 该文件定义了多种线程同步锁的封装，包括互斥锁、读写锁、自旋锁和原子锁等，
 * 并提供了RAII风格的锁管理器模板，确保锁的自动获取和释放，防止死锁和忘记释放锁的问题。
 * 所有锁类都继承自Noncopyable，禁止拷贝构造和赋值操作，确保锁的安全性。
 */

#pragma once

#include <pthread.h>

#include <atomic>
#include <list>

#include "coroutine.hpp"
#include "base/noncopyable.hpp"

namespace CIM {
/**
     * @brief 通用锁管理模板，RAII风格
     *
     * 该模板为各种锁类型提供统一的RAII管理机制，在构造时自动加锁，析构时自动解锁。
     * 支持普通锁操作，适用于Mutex、SpinLock、CASLock等互斥锁类型。
     *
     * @tparam T 锁类型
     */
template <class T>
struct ScopedLockImpl : public Noncopyable {
   public:
    ScopedLockImpl(T& mutex) : m_mutex(mutex) {
        m_mutex.lock();
        m_locked = true;
    }
    // 构造的时候加锁，用于const上下文(const 函数)
    ScopedLockImpl(const T& mutex) : m_mutex(const_cast<T&>(mutex)) {
        m_mutex.lock();
        m_locked = true;
    }
    ~ScopedLockImpl() { unlock(); }
    // 手动上锁（读锁/写锁）
    void lock() {
        if (!m_locked) {
            m_mutex.lock();
            m_locked = true;
        }
    }
    // 解锁（读锁/写锁）
    void unlock() {
        if (m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }

   private:
    T& m_mutex;
    bool m_locked;
};

/**
     * @brief 读锁管理模板
     *
     * 专门用于管理读写锁的读锁部分，提供RAII风格的读锁管理。
     * 在构造时自动加读锁，析构时自动解锁。
     *
     * @tparam T 读写锁类型
     */
template <class T>
struct ReadScopedLockImpl : public Noncopyable {
   public:
    ReadScopedLockImpl(T& mutex) : m_mutex(mutex) {
        m_mutex.rdlock();
        m_locked = true;
    }
    // 构造的时候加锁，用于const上下文
    ReadScopedLockImpl(const T& mutex) : m_mutex(const_cast<T&>(mutex)) {
        m_mutex.rdlock();
        m_locked = true;
    }
    ~ReadScopedLockImpl() { unlock(); }
    void lock() {
        if (!m_locked) {
            m_mutex.rdlock();
            m_locked = true;
        }
    }
    void unlock() {
        if (m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }

   private:
    T& m_mutex;
    bool m_locked;
};

/**
     * @brief 写锁管理模板
     *
     * 专门用于管理读写锁的写锁部分，提供RAII风格的写锁管理。
     * 在构造时自动加写锁，析构时自动解锁。
     *
     * @tparam T 读写锁类型
     */
template <class T>
struct WriteScopedLockImpl : public Noncopyable {
   public:
    // 构造的时候加锁
    WriteScopedLockImpl(T& mutex) : m_mutex(mutex) {
        m_mutex.wrlock();
        m_locked = true;
    }

    // 构造的时候加锁，用于const上下文
    WriteScopedLockImpl(const T& mutex) : m_mutex(const_cast<T&>(mutex)) {
        m_mutex.wrlock();
        m_locked = true;
    }

    // 析构的时候解锁
    ~WriteScopedLockImpl() { unlock(); }
    // 主动加锁
    void lock() {
        if (!m_locked) {
            m_mutex.wrlock();
            m_locked = true;
        }
    }
    void unlock() {
        if (m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }

   private:
    T& m_mutex;
    bool m_locked;
};

/**
     * @brief 互斥锁类
     *
     * 对pthread_mutex_t的面向对象封装，提供基本的互斥锁功能。
     * 同一时间只允许一个线程持有该锁，适用于保护临界区资源。
     */
class Mutex : public Noncopyable {
   public:
    using Lock = ScopedLockImpl<Mutex>;

    Mutex();
    ~Mutex();

    void lock();
    void unlock();

   private:
    pthread_mutex_t m_mutex;
};

/**
     * @brief 读写锁类
     *
     * 对pthread_rwlock_t的面向对象封装，提供读写锁功能。
     * 支持多个读者同时读取（共享锁），但写者独占访问（独占锁）。
     * 适用于读多写少的场景。
     */
class RWMutex : public Noncopyable {
   public:
    using ReadLock = ReadScopedLockImpl<RWMutex>;
    using WriteLock = WriteScopedLockImpl<RWMutex>;

    RWMutex();
    ~RWMutex();

    void rdlock();
    void wrlock();
    void unlock();

   private:
    pthread_rwlock_t m_mutex;
};

/**
     * @brief 自旋锁类
     *
     * 对pthread_spinlock_t的面向对象封装，提供自旋锁功能。
     * 线程在获取锁失败时不会睡眠，而是一直循环尝试获取锁（忙等待）。
     * 适用于锁持有时间很短的场景。
     */
class SpinLock : public Noncopyable {
   public:
    using Lock = ScopedLockImpl<SpinLock>;

    SpinLock();
    ~SpinLock();

    void lock();
    void unlock();

   private:
    pthread_spinlock_t m_mutex;
};

/**
     * @brief 原子锁类
     *
     * 基于std::atomic_flag实现的无锁同步机制，使用CAS操作实现。
     * 通过原子标志位实现锁功能，适用于高性能无锁编程场景。
     */
class CASLock : public Noncopyable {
   public:
    using Lock = ScopedLockImpl<CASLock>;

    CASLock();
    ~CASLock();

    void lock();
    void unlock();

   private:
    volatile std::atomic_flag m_mutex;
};

/**
     * @brief 空锁类
     *
     * 不执行任何实际锁定操作的空实现，主要用于性能测试和调试。
     * 可以在测试中替换真实锁来测量锁操作的开销。
     */
class NullMutex : public Noncopyable {
   public:
    using Lock = ScopedLockImpl<NullMutex>;

    NullMutex() {}
    ~NullMutex() {}

    void lock() {}
    void unlock() {}
};

/**
     * @brief 空读写锁类
     *
     * 不执行任何实际锁定操作的空实现，主要用于性能测试和调试。
     * 可以在测试中替换真实读写锁来测量锁操作的开销。
     */
class NullRWMutex : public Noncopyable {
   public:
    using ReadLock = ReadScopedLockImpl<NullRWMutex>;
    using WriteLock = WriteScopedLockImpl<NullRWMutex>;

    NullRWMutex() {}
    ~NullRWMutex() {}

    void rdlock() {}
    void wrlock() {}
    void unlock() {}
};

class Scheduler;
class CoroutineSemaphore : Noncopyable {
   public:
    typedef SpinLock MutexType;

    CoroutineSemaphore(size_t initial_concurrency = 0);
    ~CoroutineSemaphore();

    bool tryWait();
    void wait();
    void notify();

    size_t getConcurrency() const { return m_concurrency; }
    void reset() { m_concurrency = 0; }

   private:
    MutexType m_mutex;
    std::list<std::pair<Scheduler*, Coroutine::ptr>> m_waiters;
    size_t m_concurrency;
};
}  // namespace CIM