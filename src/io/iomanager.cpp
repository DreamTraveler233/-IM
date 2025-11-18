#include "io/iomanager.hpp"

#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cerrno>

#include "net/fd_manager.hpp"
#include "base/macro.hpp"

namespace CIM {
static auto g_logger = CIM_LOG_NAME("system");

IOManager::IOManager(size_t threads, bool use_caller, const std::string& name)
    : Scheduler(threads, use_caller, name) {
    int saved_errno;
    // 创建 epoll 实例，用于监听文件描述符事件
    FileDescriptor epfd(epoll_create1(EPOLL_CLOEXEC));  // 避免在子进程中继承文件描述符
    if (!epfd.isValid()) {
        saved_errno = errno;
        CIM_LOG_ERROR(g_logger) << "epoll_create1 failed: " << strerror(saved_errno);
        throw std::runtime_error("IOManager initialization failed");
    }

    // 创建管道，用于唤醒调度器
    int pipe_fd[2];
    int rt = pipe(pipe_fd);
    if (-1 == rt) {
        saved_errno = errno;
        CIM_LOG_ERROR(g_logger) << "pipe failed: " << strerror(saved_errno);
        throw std::runtime_error("IOManager initialization failed");
    }

    FileDescriptor pipe_read_fd(pipe_fd[0]);
    FileDescriptor pipe_write_fd(pipe_fd[1]);

    epoll_event ev = {};
    ev.events = EPOLLIN | EPOLLET;    // 使用 ET 模式监听读事件
    ev.data.fd = pipe_read_fd.get();  // 关联管道的读端

    // 将管道的读端设置为非阻塞模式，避免在读取时发生阻塞（若无数据可读或者是无法写入则立即返回）
    rt = fcntl(pipe_read_fd.get(), F_SETFL, O_NONBLOCK);
    if (-1 == rt) {
        saved_errno = errno;
        CIM_LOG_ERROR(g_logger) << "fcntl failed: " << strerror(saved_errno);
        throw std::runtime_error("IOManager initialization failed");
    }

    // 将管道的读端添加到 epoll 实例中，以监听其事件
    rt = epoll_ctl(epfd.get(), EPOLL_CTL_ADD, pipe_read_fd.get(), &ev);
    if (-1 == rt) {
        saved_errno = errno;
        CIM_LOG_ERROR(g_logger) << "epoll_ctl failed: " << strerror(saved_errno);
        throw std::runtime_error("IOManager initialization failed");
    }

    // 所有资源初始化成功，释放所有权并保存到成员变量中
    m_epfd = epfd.release();
    m_tickleFds[0] = pipe_read_fd.release();
    m_tickleFds[1] = pipe_write_fd.release();

    // 初始化上下文存储容量，确保可以容纳足够的上下文对象
    contextResize(64);

    // 启动调度器，开始任务调度
    start();
}

IOManager::~IOManager() {
    stop();

    // 关闭 epoll 文件描述符和管道文件描述符
    close(m_epfd);
    close(m_tickleFds[0]);
    close(m_tickleFds[1]);

    // 遍历并释放所有的 FdContext 对象
    for (size_t i = 0; i < m_fdContexts.size(); ++i) {
        if (m_fdContexts[i]) {
            delete m_fdContexts[i];
        }
    }
}

bool IOManager::addEvent(int fd, Event event, std::function<void()> cb) {
    CIM_ASSERT(fd >= 0)
    CIM_ASSERT(event == READ || event == WRITE);
    CIM_ASSERT(cb || Coroutine::GetThis());

    FdContext* fd_ctx = nullptr;

    // ====================取出对应 fd 的上下文====================
    RWMutexType::ReadLock lock(m_mutex);
    if ((int)m_fdContexts.size() > fd)  // 空间够
    {
        fd_ctx = m_fdContexts[fd];
        lock.unlock();
    } else  // 空间不足，进行扩容，再取出对应fd上下文
    {
        lock.unlock();

        RWMutexType::WriteLock lock2(m_mutex);
        contextResize(fd * 1.5);
        fd_ctx = m_fdContexts[fd];
    }

    // ====================将事件注册到epoll实例中====================
    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if (fd_ctx->events & event) {
        // 如果事件已经存在，则直接返回true，避免重复添加相同的事件类型
        CIM_LOG_DEBUG(g_logger) << "addEvent assert fd=" << fd << " event=" << event
                                << " fd_ctx.event=" << fd_ctx->events;
        return true;
    }

    // 根据已有事件决定是新增还是修改 epoll 监控
    int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    epoll_event ev = {};
    ev.events = fd_ctx->events | event | EPOLLET;  // 设置边缘触发模式
    ev.data.ptr = fd_ctx;

    int saved_errno;
    // 调用 epoll_ctl 更新事件监听
    int rt = epoll_ctl(m_epfd, op, fd, &ev);
    if (rt) {
        saved_errno = errno;
        // 如果 epoll_ctl 失败，记录错误日志并返回失败
        CIM_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", " << op << ", " << fd << ", "
                                << ev.events << "): " << rt << " (" << saved_errno << ") ("
                                << strerror(saved_errno) << ")";
        return false;
    }

    // ====================更新上下文信息====================
    // 更新事件计数及上下文中的事件掩码
    ++m_pendingEventCount;
    fd_ctx->events = (Event)(fd_ctx->events | event);

    // 初始化事件上下文
    FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
    if (event_ctx.scheduler || event_ctx.coroutine || event_ctx.cb) {
        CIM_LOG_WARN(g_logger) << "addEvent warning fd=" << fd << " event=" << event
                               << " event_ctx.scheduler="
                               << (event_ctx.scheduler ? "not null" : "null")
                               << " event_ctx.coroutine="
                               << (event_ctx.coroutine ? "not null" : "null")
                               << " event_ctx.cb=" << (event_ctx.cb ? "not null" : "null");
    }

    event_ctx.scheduler = Scheduler::GetThis();
    if (cb) {
        // 如果提供了回调函数，则绑定到事件上下文中
        event_ctx.cb.swap(cb);
    } else {
        // 否则使用当前协程作为事件处理逻辑
        event_ctx.coroutine = Coroutine::GetThis();
        CIM_ASSERT(event_ctx.coroutine->getState() == Coroutine::EXEC);
    }

    return true;
}

bool IOManager::delEvent(int fd, Event event) {
    CIM_ASSERT(fd >= 0)
    CIM_ASSERT(event == READ || event == WRITE);

    FdContext* fd_ctx = nullptr;

    // ====================获取文件描述符对应的 FdContext====================
    {
        // 加读锁以保护 m_fdContexts 的并发访问
        RWMutexType::ReadLock lock(m_mutex);
        if ((int)m_fdContexts.size() <= fd) {
            CIM_LOG_ERROR(g_logger) << "delEvent: fd=" << fd
                                    << " out of range, m_fdContexts.size()=" << m_fdContexts.size();
            return false;  // 文件描述符超出范围，直接返回失败
        }

        fd_ctx = m_fdContexts[fd];
    }

    // ====================判断删除的事件是否存在====================
    // 对 fd_ctx 加锁，确保对其事件的修改是线程安全的
    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if (!(fd_ctx->events & event)) {
        return false;  // 如果目标事件不存在，直接返回失败
    }

    // ====================从epoll中删除事件监听====================
    // 计算新的事件集合，并决定 epoll 操作类型
    Event new_events = (Event)(fd_ctx->events & ~event);  // 移除指定事件
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;  // 根据剩余事件决定修改还是删除
    epoll_event ev = {};
    ev.events = new_events | EPOLLET;  // 设置边缘触发模式
    ev.data.ptr = fd_ctx;

    int saved_errno;
    // 调用 epoll_ctl 更新事件监听
    int rt = epoll_ctl(m_epfd, op, fd, &ev);
    if (rt) {
        saved_errno = errno;
        // 如果 epoll_ctl 失败，记录错误日志并返回失败
        CIM_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", " << op << ", " << fd << ", "
                                << ev.events << "): " << rt << " (" << saved_errno << ") ("
                                << strerror(saved_errno) << ")";
        return false;
    }

    // ====================更新上下文信息====================
    --m_pendingEventCount;        // 减少待处理事件计数
    fd_ctx->events = new_events;  // 更新文件描述符上下文中的事件集合

    // 重置与目标事件相关的上下文
    FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
    fd_ctx->resetContext(event_ctx);
    return true;  // 成功删除事件
}

bool IOManager::cancelEvent(int fd, Event event) {
    CIM_ASSERT(fd >= 0)
    CIM_ASSERT(event == READ || event == WRITE);

    FdContext* fd_ctx = nullptr;
    {
        // 加读锁以保护m_fdContexts容器的并发访问
        RWMutexType::ReadLock lock(m_mutex);
        if ((int)m_fdContexts.size() <= fd) {
            CIM_LOG_ERROR(g_logger) << "cancelEvent: fd=" << fd
                                    << " out of range, m_fdContexts.size()=" << m_fdContexts.size();
            return false;  // 文件描述符超出范围，直接返回false
        }

        fd_ctx = m_fdContexts[fd];
    }

    // 对文件描述符上下文加锁，确保对其事件掩码的安全访问
    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if (!(fd_ctx->events & event)) {
        return false;  // 如果当前文件描述符未监听该事件，直接返回false
    }

    // 计算新的事件集合，并确定epoll操作类型
    Event new_events = (Event)(fd_ctx->events & ~event);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event ev = {};
    ev.events = new_events | EPOLLET;
    ev.data.ptr = fd_ctx;

    int saved_errno;
    // 使用epoll_ctl修改或删除事件监听
    int rt = epoll_ctl(m_epfd, op, fd, &ev);
    if (rt) {
        saved_errno = errno;
        CIM_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", " << op << ", " << fd << ", "
                                << ev.events << "): " << rt << " (" << saved_errno << ") ("
                                << strerror(saved_errno) << ")";
        return false;  // epoll_ctl调用失败时记录错误日志并返回false
    }

    // 触发相关事件的回调函数，并减少待处理事件计数
    fd_ctx->triggerEvent(event);
    --m_pendingEventCount;

    return true;  // 返回true表示事件已成功取消
}

bool IOManager::cancelAll(int fd) {
    CIM_ASSERT(fd >= 0)

    FdContext* fd_ctx = nullptr;
    {
        RWMutexType::ReadLock lock(m_mutex);
        // 检查 fd 是否在合法范围内
        if ((int)m_fdContexts.size() <= fd) {
            CIM_LOG_ERROR(g_logger) << "cancelAll: fd=" << fd
                                    << " out of range, m_fdContexts.size()=" << m_fdContexts.size();
            return false;
        }

        fd_ctx = m_fdContexts[fd];
    }

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    // 如果 fd 上未注册任何事件，直接返回 false
    if (!(fd_ctx->events)) {
        return false;
    }

    // 从 epoll 实例中删除该文件描述符的事件监听
    int op = EPOLL_CTL_DEL;
    epoll_event ev = {};
    ev.events = 0;
    ev.data.ptr = fd_ctx;

    int saved_errno;
    int rt = epoll_ctl(m_epfd, op, fd, &ev);
    if (rt) {
        saved_errno = errno;
        CIM_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", " << op << ", " << fd << ", "
                                << ev.events << "): " << rt << " (" << saved_errno << ") ("
                                << strerror(saved_errno) << ")";
        return false;
    }

    // 触发 READ 事件的回调函数（如果已注册）
    if (fd_ctx->events & Event::READ) {
        fd_ctx->triggerEvent(READ);
        --m_pendingEventCount;
    }

    // 触发 WRITE 事件的回调函数（如果已注册）
    if (fd_ctx->events & Event::WRITE) {
        fd_ctx->triggerEvent(WRITE);
        --m_pendingEventCount;
    }

    // 确保所有事件都已被正确清理
    CIM_ASSERT(fd_ctx->events == 0)
    return true;
}

IOManager* IOManager::GetThis() {
    return dynamic_cast<IOManager*>(Scheduler::GetThis());
}

void IOManager::tickle() {
    //  检查是否有空闲线程，若无则直接返回
    if (!hasIdleThreads()) {
        return;
    }
    // 通过管道写入一个字节 "T" 以触发调度器
    int rt = write(m_tickleFds[1], "T", 1);
    CIM_ASSERT(rt == 1)
}

bool IOManager::stopping(uint64_t& timeout) {
    timeout = getNextTimer();
    // ~0ull表示没有定时器或者无限超时
    return timeout == ~0ull && m_pendingEventCount == 0 && Scheduler::stopping();
}

bool IOManager::stopping() {
    uint64_t timeout;
    return stopping(timeout);
}

void IOManager::idle() {
    // ==========初始化阶段==========
    CIM_LOG_DEBUG(g_logger) << "idle";

    // 分配 epoll_event 数组并使用智能指针管理内存，用于存储 epoll 等待到的事件
    epoll_event* events = new epoll_event[64]();
    std::shared_ptr<epoll_event> shared_event(events, [](epoll_event* ptr) { delete[] ptr; });

    // 主空闲循环，持续运行直到满足停止条件
    while (true) {
        // ==========停止条件检查==========
        // 检查是否应该停止，并获取下一个定时器超时时间
        uint64_t next_timeout = 0;
        if (stopping(next_timeout)) {
            CIM_LOG_INFO(g_logger) << "name=" << getName() << " idle stopping exit";
            break;
        }

        // ==========epoll_wait 等待事件==========
        int rt = 0;
        do {
            static const int MAX_TIMEOUT = 3000;  // 设置最大超时时间为3秒
            if (next_timeout != ~0ull)            // 如果有定时器
            {
                // 限制超时时间不超过 MAX_TIMEOUT 毫秒
                next_timeout = (int)next_timeout > MAX_TIMEOUT ? MAX_TIMEOUT : next_timeout;
            } else {
                next_timeout = MAX_TIMEOUT;
            }
            // 等待 epoll 事件，超时时间为 next_timeout 毫秒，确保定时任务能够及时执行
            rt = epoll_wait(m_epfd, events, 64, (int)next_timeout);
        } while (rt < 0 && errno == EINTR);

        // ==========处理到期定时器==========
        std::vector<std::function<void()>> cbs;
        listExpiredCb(cbs);
        if (!cbs.empty()) {
            // 将到期的定时器回调加入调度队列
            schedule(cbs.begin(), cbs.end());
            cbs.clear();
        }

        // ==========处理 epoll 事件==========
        for (int i = 0; i < rt; ++i) {
            epoll_event& event = events[i];

            // 如果事件来自用于唤醒调度器的管道，清空管道中的数据
            if (event.data.fd == m_tickleFds[0]) {
                uint8_t dummy;
                while (read(m_tickleFds[0], &dummy, 1) == 1);
                continue;
            }

            // 获取与文件描述符关联的上下文对象
            FdContext* fd_ctx = (FdContext*)event.data.ptr;
            FdContext::MutexType::Lock lock(fd_ctx->mutex);

            /*如果发生错误或挂起，启用读和写的监控，
                保证即使在错误或挂起的情况下，
                仍然能够处理 fd 上可能存在的未读或未写数据
                */
            if (event.events & (EPOLLERR | EPOLLHUP)) {
                event.events |= (EPOLLIN | EPOLLOUT) & fd_ctx->events;
            }

            // 确定实际发生的事件类型（读/写）
            int real_events = NONE;
            if (event.events & EPOLLIN) {
                real_events |= READ;
            }
            if (event.events & EPOLLOUT) {
                real_events |= WRITE;
            }

            // 如果没有设置相关事件则跳过触发事件
            if ((fd_ctx->events & real_events) == NONE) {
                continue;
            }

            // 更新 epoll 监听事件：如果有剩余事件则修改(MOD)，否则删除(DEL)
            int left_events = (fd_ctx->events & ~real_events);
            int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
            event.events = left_events | EPOLLET;

            // 更新 epoll 对该文件描述符的监听设置
            int rt2 = epoll_ctl(m_epfd, op, fd_ctx->fd, &event);
            if (rt2) {
                int saved_errno = errno;
                CIM_LOG_ERROR(g_logger)
                    << "epoll_ctl(" << m_epfd << ", " << op << ", " << fd_ctx->fd << ", "
                    << event.events << "): " << rt2 << " (" << saved_errno << ") ("
                    << strerror(saved_errno) << ")";
                continue;
            }

            // 如果适用，则触发读事件回调
            if (real_events & READ) {
                fd_ctx->triggerEvent(READ);
                --m_pendingEventCount;
            }

            // 如果适用，则触发写事件回调
            if (real_events & WRITE) {
                fd_ctx->triggerEvent(WRITE);
                --m_pendingEventCount;
            }
        }

        // ==========协程切换==========
        // 将控制权交回协程调度器，当前协程让出执行权
        Coroutine::ptr cur = Coroutine::GetThis();
        auto raw_ptr = cur.get();
        cur.reset();  // 引用计数减一
        raw_ptr->swapOut();
    }
}

void IOManager::onTimerInsertedAtFront() {
    tickle();
}

void IOManager::contextResize(size_t size) {
    m_fdContexts.resize(size);

    // 遍历数组，为未初始化的元素分配和设置文件描述符上下文
    for (size_t i = 0; i < m_fdContexts.size(); ++i) {
        if (!m_fdContexts[i])  // 如果当前索引的元素为空
        {
            m_fdContexts[i] = new FdContext;
            m_fdContexts[i]->fd = i;  // 设置文件描述符
        }
    }
}

IOManager::FdContext::EventContext& IOManager::FdContext::getContext(Event event) {
    switch (event) {
        case Event::READ:
            // 返回读事件上下文
            return read;
        case Event::WRITE:
            // 返回写事件上下文
            return write;
        default:
            CIM_ASSERT2(false, "getContext");
    }
    throw std::invalid_argument("getContext invalid event");
}

void IOManager::FdContext::resetContext(EventContext& event) {
    event.scheduler = nullptr;  // 将调度器指针重置为 nullptr，表示该事件不再绑定到任何调度器。
    event.coroutine.reset();    // 重置协程上下文，释放与其关联的资源。
    event.cb = nullptr;         // 清空回调函数指针，确保不会保留任何旧的回调逻辑。
}

void IOManager::FdContext::triggerEvent(Event event) {
    // 确保传入的事件是当前已注册的事件之一
    CIM_ASSERT(events & event);

    // 清除已触发事件的标志位
    events = (Event)(events & ~event);

    // 获取与事件关联的上下文
    EventContext& event_ctx = getContext(event);

    // 根据上下文内容调度回调函数或协程
    if (event_ctx.cb) {
        // 如果存在回调函数，则调度该回调
        event_ctx.scheduler->schedule(event_ctx.cb);
    } else {
        // 如果不存在回调函数，则调度协程
        event_ctx.scheduler->schedule(event_ctx.coroutine);
    }

    // 触发完成后必须清理上下文，避免后续 addEvent 命中断言
    // 尤其是在 fd_ctx->events 位已清零但上下文仍残留时
    resetContext(event_ctx);

    return;
}
}  // namespace CIM