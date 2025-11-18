/**
 * @file semaphore.hpp
 * @brief 信号量封装类
 * @author szy
 *
 * 该文件定义了对 POSIX 信号量的 C++ 封装，提供了一种线程同步机制。
 * 信号量是一种用于控制多个线程对共享资源访问的同步原语，常用于线程间的协调和同步。
 */

#pragma once

#include <semaphore.h>
#include <stdint.h>

#include "base/noncopyable.hpp"

namespace CIM {
/**
     * @brief 信号量类，对POSIX信号量进行封装
     *
     * 信号量是一种用于多线程编程中的同步机制，它可以用来控制对共享资源的访问。
     * 该类通过封装POSIX信号量(sem_t)实现，提供了wait()和notify()两个基本操作。
     */
class Semaphore : public Noncopyable {
   public:
    /**
         * @brief 构造函数
         * @param[in] count 信号量初始值，默认为0
         *
         * 初始化信号量，count参数指定信号量的初始值。
         * 如果count为0，表示初始时没有资源可用；
         * 如果count大于0，表示初始时有count个资源可用。
         */
    Semaphore(uint32_t count = 0);

    /**
         * @brief 析构函数
         *
         * 销毁信号量资源，释放相关系统资源。
         */
    ~Semaphore();

    /**
         * @brief 等待操作（P操作）
         *
         * 如果信号量值大于0，则将信号量值减1并立即返回；
         * 如果信号量值等于0，则阻塞等待直到信号量值大于0。
         * 该操作是原子性的。
         */
    void wait();

    /**
         * @brief 通知操作（V操作）
         *
         * 将信号量值加1，如果有线程在wait()中阻塞等待，则唤醒其中一个线程。
         * 该操作是原子性的。
         */
    void notify();

   private:
    sem_t m_semaphore;  // POSIX信号量
};
}  // namespace CIM