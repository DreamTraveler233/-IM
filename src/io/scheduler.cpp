#include "io/scheduler.hpp"

#include "net/hook.hpp"
#include "base/macro.hpp"

namespace CIM {
static auto g_logger = CIM_LOG_NAME("system");

// 当前线程的调度器对象
static thread_local Scheduler* t_scheduler = nullptr;
// 当前线程的协程对象
static thread_local Coroutine* t_coroutine = nullptr;

Scheduler::Scheduler(size_t threads, bool use_caller, const std::string& name) : m_name(name) {
    CIM_ASSERT(threads > 0);

    // 如果使用调用线程，则将当前线程作为调度线程之一
    if (use_caller) {
        Coroutine::GetThis();  // 初始化当前线程的主协程
        --threads;

        CIM_ASSERT(GetThis() == nullptr);  // 当前线程的调度器实例为空
        t_scheduler = this;                // 设置当前线程的调度器

        // 创建根协程并绑定到当前调度器的run方法
        m_rootCoroutine.reset(new Coroutine(std::bind(&Scheduler::run, this), 0, true));
        Thread::SetName(m_name);  // 调用线程

        m_rootThreadId = GetThreadId();  // 记录主线程ID
        m_threadIds.push_back(m_rootThreadId);
    } else {
        m_rootThreadId = -1;
    }
    m_threadCount = threads;
}

Scheduler::~Scheduler() {
    CIM_ASSERT(!m_isRunning);
    if (GetThis() == this) {
        t_scheduler = nullptr;
    }
}

const std::string& Scheduler::getName() const {
    return m_name;
}

Scheduler* Scheduler::GetThis() {
    return t_scheduler;
}
void Scheduler::setThis() {
    t_scheduler = this;
}

bool Scheduler::hasIdleThreads() {
    return m_idleThreadCount > 0;
}

Coroutine* Scheduler::GetMainCoroutine() {
    return t_coroutine;
}

void Scheduler::start() {
    MutexType::Lock lock(m_mutex);

    // 确保调度器只能启动一次
    if (m_isRunning) {
        return;
    }
    m_isRunning = true;

    CIM_ASSERT(m_threads.empty());
    m_threads.resize(m_threadCount);  // 提前分配内存
    for (size_t i = 0; i < m_threadCount; ++i) {
        // 创建工作线程，绑定调度器的run方法作为线程执行函数
        m_threads[i].reset(
            new Thread(std::bind(&Scheduler::run, this), m_name + "_" + std::to_string(i)));
        m_threadIds.push_back(m_threads[i]->getId());
    }

    lock.unlock();
}

void Scheduler::stop() {
    m_autoStop = true;

    // 快速停止条件：没有工作线程、存在根协程且根协程处于终止或初始状态
    if (m_threadCount == 0 && m_rootCoroutine &&
        (m_rootCoroutine->getState() == Coroutine::State::TERM ||
         m_rootCoroutine->getState() == Coroutine::State::INIT)) {
        CIM_LOG_INFO(g_logger) << "quick stopped";
        m_isRunning = false;

        // 再次确认是否应该停止
        if (stopping()) {
            return;
        }
    }

    // 根据是否使用调用线程进行断言检查
    if (m_rootThreadId != -1) {
        CIM_ASSERT(GetThis() == this);
    } else {
        CIM_ASSERT(GetThis() != this);
    }

    m_isRunning = false;

    // 确保在线程安全的前提下唤醒所有工作线程
    {
        MutexType::Lock lock(m_mutex);
        for (size_t i = 0; i < m_threadCount; ++i) {
            CIM_LOG_DEBUG(g_logger) << "worker thread tickle";
            tickle();
        }
    }

    // 处理根协程
    if (m_rootCoroutine) {
        CIM_LOG_DEBUG(g_logger) << "m_rootCoroutine tickle";
        tickle();

        // 在调用根协程之前再次检查是否应该停止
        if (!stopping()) {
            m_rootCoroutine->call();
        }
    }

    // 收集线程指针并等待线程结束
    std::vector<Thread::ptr> thrs;
    {
        MutexType::Lock lock(m_mutex);
        thrs.swap(m_threads);
    }

    // 等待所有工作线程结束
    for (auto& i : thrs) {
        i->join();
    }
}

void Scheduler::switchTo(int thread) {
    CIM_ASSERT(Scheduler::GetThis() != nullptr);
    if (Scheduler::GetThis() == this) {
        if (thread == -1 || thread == CIM::GetThreadId()) {
            return;
        }
    }
    schedule(Coroutine::GetThis(), thread);
    Coroutine::YieldToHold();
}

void Scheduler::tickle() {
    CIM_LOG_INFO(g_logger) << "tickle";
}

bool Scheduler::stopping() {
    MutexType::Lock lock(m_mutex);
    return m_autoStop && m_taskQueue.empty() && !m_isRunning && m_activeThreadCount == 0;
}

void Scheduler::idle() {
    CIM_LOG_INFO(g_logger) << "thread idle";
    while (!stopping()) {
        Coroutine::YieldToHold();
    }
}

void Scheduler::run() {
    // 启动HOOK
    CIM::set_hook_enable(true);

    setThis();  // 设置当前线程的调度器实例

    // 创建工作线程的主协程
    if (GetThreadId() != m_rootThreadId) {
        t_coroutine = Coroutine::GetThis().get();
    } else {
        t_coroutine = m_rootCoroutine.get();
    }

    // 创建空闲协程，当没有任务可执行时运行
    Coroutine::ptr idle_coroutine(new Coroutine(std::bind(&Scheduler::idle, this)));
    // 用于执行回调函数的协程
    Coroutine::ptr cb_coroutine;

    // 存储从协程队列中取出的协程或回调任务
    Task task;

    while (true) {
        // ==========任务获取阶段==========
        task.reset();            // 清除上一次循环中保存的任务，确保当前循环处理的是新任务
        bool tickle_me = false;  // 是否需要通知其他线程
        bool is_active = false;  // 线程是否处于活动状态
        {
            // 加锁访问协程队列
            MutexType::Lock lock(m_mutex);
            auto it = m_taskQueue.begin();
            while (it != m_taskQueue.end()) {
                // 当前任务指定了执行线程，且该线程不是当前线程，则跳过该任务，并标记为需要通知其他线程
                if (it->threadId != -1 && it->threadId != GetThreadId()) {
                    ++it;
                    tickle_me = true;
                    continue;
                }

                CIM_ASSERT(it->coroutine || it->cb);

                // 如果it中保存的是协程，并且正在执行中，则跳过
                if (it->coroutine && it->coroutine->getState() == Coroutine::State::EXEC) {
                    ++it;
                    continue;
                }

                // 取出协程任务
                task = *it;
                m_taskQueue.erase(it);
                ++m_activeThreadCount;
                is_active = true;
                break;
            }
        }

        // ==========跨线程通知阶段==========
        // 如果有其他线程需要被唤醒，则触发tickle
        if (tickle_me) {
            tickle();
        }

        // ==========任务处理阶段==========
        if (task.coroutine &&  // 协程类型任务
            task.coroutine->getState() != Coroutine::State::TERM &&
            task.coroutine->getState() != Coroutine::State::EXCEPT) {
            // 进入目标协程
            task.coroutine->swapIn();
            // 离开目标协程
            --m_activeThreadCount;
            // 如果协程状态为READY，说明协程主动让出了执行权，但仍需要继续执行，重新加入调度队列
            if (task.coroutine->getState() == Coroutine::State::READY) {
                schedule(task.coroutine);
            }
            // 如果协程未结束且无异常，设置为HOLD状态
            else if (task.coroutine->getState() != Coroutine::State::TERM &&
                     task.coroutine->getState() != Coroutine::State::EXCEPT) {
                task.coroutine->setState(Coroutine::State::HOLD);
            }
            // 如果协程是终止状态（TERM）或异常状态（EXCEPT），结束该协程的任务
        } else if (task.cb)  // 回调函数类型任务
        {
            // 回调协程复用
            if (cb_coroutine) {
                cb_coroutine->reset(task.cb);
            } else {
                cb_coroutine.reset(new Coroutine(task.cb));
            }
            // 进入回调函数
            cb_coroutine->swapIn();
            // 离开回调函数
            --m_activeThreadCount;

            // 如果协程状态为READY，重新加入调度队列
            if (cb_coroutine->getState() == Coroutine::State::READY) {
                schedule(cb_coroutine);
                cb_coroutine.reset();
            } else if (cb_coroutine->getState() == Coroutine::State::TERM ||
                       cb_coroutine->getState() == Coroutine::State::EXCEPT) {
                cb_coroutine->reset(nullptr);
            }
            // 其他情况设置为HOLD状态并重置协程指针
            else {
                cb_coroutine->setState(Coroutine::State::HOLD);
                cb_coroutine.reset();
            }
        }
        // ==========空闲处理阶段==========
        else {
            // 任务队列不为空，但是没有取到合适的任务
            if (is_active) {
                --m_activeThreadCount;
                continue;
            }

            // 空闲协程已经执行完毕
            if (idle_coroutine->getState() == Coroutine::State::TERM) {
                CIM_LOG_INFO(g_logger) << "idle coroutine over";
                break;
            }

            ++m_idleThreadCount;
            idle_coroutine->swapIn();
            --m_idleThreadCount;

            // 如果空闲协程未结束且无异常，设置为HOLD状态
            if (idle_coroutine->getState() != Coroutine::State::EXCEPT &&
                idle_coroutine->getState() != Coroutine::State::TERM) {
                idle_coroutine->setState(Coroutine::State::HOLD);
            }
        }
    }
}

SchedulerSwitcher::SchedulerSwitcher(Scheduler* target) {
    m_caller = Scheduler::GetThis();
    if (target) {
        target->switchTo();
    }
}

SchedulerSwitcher::~SchedulerSwitcher() {
    if (m_caller) {
        m_caller->switchTo();
    }
}

std::ostream& Scheduler::dump(std::ostream& os) {
    os << "[Scheduler name=" << m_name << " size=" << m_threadCount
       << " active_count=" << m_activeThreadCount << " idle_count=" << m_idleThreadCount
       << " Running=" << m_isRunning << " ]" << std::endl
       << "    ";
    for (size_t i = 0; i < m_threadIds.size(); ++i) {
        if (i) {
            os << ", ";
        }
        os << m_threadIds[i];
    }
    return os;
}
}  // namespace CIM