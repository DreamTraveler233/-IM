#include "net/hook.hpp"

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include "config/config.hpp"
#include "io/coroutine.hpp"
#include "net/fd_manager.hpp"
#include "io/iomanager.hpp"
#include "base/macro.hpp"
#include "io/scheduler.hpp"

namespace CIM {
Logger::ptr g_logger = CIM_LOG_NAME("system");

// TCP 超时时间
static auto g_tcp_connect_timeout =
    Config::Lookup("tcp.connect.timeout", 5000, "tcp connect timeout");

// hook 是否启用
static thread_local bool t_hook_enable = false;

// 定义需要hook的函数列表
#define HOOK_FUN(XX) \
    XX(sleep)        \
    XX(usleep)       \
    XX(nanosleep)    \
    XX(socket)       \
    XX(connect)      \
    XX(accept)       \
    XX(read)         \
    XX(readv)        \
    XX(recv)         \
    XX(recvfrom)     \
    XX(recvmsg)      \
    XX(write)        \
    XX(writev)       \
    XX(send)         \
    XX(sendto)       \
    XX(sendmsg)      \
    XX(close)        \
    XX(fcntl)        \
    XX(ioctl)        \
    XX(getsockopt)   \
    XX(setsockopt)

/**
     * @brief 初始化hook系统，通过dlsym获取原始系统函数地址
     *
     * @details 该函数只会被调用一次，用于获取所有需要hook的系统函数的原始地址，
     *          并保存在对应的函数指针变量中，以便后续在hook函数中调用原始函数。
     */
void hook_init() {
    // 保证只初始化一次
    static bool is_inited = false;
    if (is_inited) {
        return;
    }

    // 获取原始函数地址
#define XX(name) name##_f = (name##_fun)dlsym(RTLD_NEXT, #name);
    HOOK_FUN(XX)
#undef XX
}

static uint64_t s_connect_timeout = 0;
// 静态初始化器，确保程序启动时就完成hook初始化
struct HookIniter {
    HookIniter() {
        hook_init();
        s_connect_timeout = g_tcp_connect_timeout->getValue();

        g_tcp_connect_timeout->addListener([](const int& old_value, const int& new_value) {
            CIM_LOG_INFO(g_logger)
                << "tcp connect timeout changed from " << old_value << " to " << new_value;
            s_connect_timeout = new_value;
        });
    }
};

static HookIniter s_hook_initer;

/**
     * @brief 检查当前线程是否启用了hook功能
     * @return bool true表示启用，false表示未启用
     */
bool is_hook_enable() {
    return t_hook_enable;
}

/**
     * @brief 设置当前线程的hook启用状态
     * @param[in] flag true表示启用hook，false表示禁用hook
     */
void set_hook_enable(bool flag) {
    t_hook_enable = flag;
}

// 用于跟踪定时器的状态
// cancelled值为0表示未取消，ETIMEDOUT表示超时取消
struct timer_info {
    int cancelled = 0;
};

/**
     * @brief 执行带有协程支持的IO操作
     * @tparam OriginFun 原始函数类型
     * @tparam Args 函数参数包类型
     * @param fd 文件描述符
     * @param fun 原始系统调用函数指针
     * @param hook_fun_name 被hook的函数名称
     * @param event IO事件类型（READ/WRITE）
     * @param timeout_so 超时设置选项（SO_RCVTIMEO/SO_SNDTIMEO）
     * @param args 传递给原始函数的参数包
     * @return 返回IO操作结果，成功返回传输字节数，失败返回-1并设置errno
     *
     * 该函数是所有IO相关hook函数的核心实现，提供以下功能：
     * 1. 检查hook是否启用，未启用则直接调用原始函数
     * 2. 获取文件描述符上下文信息
     * 3. 处理非阻塞IO操作
     * 4. 在IO阻塞时将当前协程挂起，并注册相应的事件监听
     * 5. 支持超时控制
     */
template <typename OriginFun, typename... Args>
static ssize_t do_io(int fd, OriginFun fun, const char* hook_fun_name, uint32_t event,
                     int timeout_so, Args&&... args) {
    // ==========判断是否启用hook==========
    if (!is_hook_enable()) {
        return fun(fd, std::forward<Args>(args)...);
    }

    // ==========获取文件描述符上下文==========
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(fd);
    if (!ctx) {
        return fun(fd, std::forward<Args>(args)...);
    }

    if (ctx->isClose()) {
        // 如果文件描述符已经关闭，设置错误码并返回
        errno = EBADF;
        return -1;
    }

    if (!ctx->isSocket() || ctx->getUserNonBlock()) {
        // 如果不是套接字或者用户设置了非阻塞模式，则直接调用原始函数
        return fun(fd, std::forward<Args>(args)...);
    }

    // 获取超时设置
    uint64_t timeout = ctx->getTimeout(timeout_so);
    std::shared_ptr<timer_info> tinfo(new timer_info);

// 重试标签，用于IO操作被中断或需要重试的情况
retry:
    // 尝试执行原始IO操作
    ssize_t n = fun(fd, std::forward<Args>(args)...);
    // 如果被信号中断，则重试
    while (n == -1 && errno == EINTR) {
        n = fun(fd, std::forward<Args>(args)...);
    }

    // 如果是因为缓冲区无数据/无法写入导致的阻塞
    if (n == -1 && errno == EAGAIN) {
        IOManager* iom = IOManager::GetThis();
        Timer::ptr timer;
        std::weak_ptr<timer_info> winfo(tinfo);

        if (timeout != (uint64_t)-1)  // 判断是否设置了超时时间
        {
            // 设置一个条件定时器来实现超时控制
            timer = iom->addConditionTimer(
                timeout,
                [winfo, fd, iom, event]() {
                    auto t = winfo.lock();
                    // 如果转换失败或者cancelled已被设置，直接返回
                    if (!t || t->cancelled) {
                        return;
                    }
                    // 记录超时标准
                    t->cancelled = ETIMEDOUT;
                    // 通知 IOManager 取消对指定文件描述符上特定事件的监听
                    iom->cancelEvent(fd, (IOManager::Event)event);
                },
                winfo);
        }

        // 添加IO事件监听
        bool rt = iom->addEvent(fd, (IOManager::Event)event);
        if (!rt) {
            // 添加事件失败，记录日志并返回错误
            CIM_LOG_ERROR(g_logger) << hook_fun_name << " addEvent (" << fd << ", " << event << ")";
            if (timer) {
                timer->cancel();
            }
            return -1;
        }

        // 成功添加事件，让出当前协程控制权
        Coroutine::YieldToHold();

        // 协程重新被调度时，首先取消定时器（因为不再需要超时控制）
        if (timer) {
            timer->cancel();
        }

        // 如果定时器触发（超时），设置错误码并返回
        if (tinfo->cancelled) {
            errno = tinfo->cancelled;
            return -1;
        }
        // 重新尝试IO操作
        goto retry;
    }
    return n;
}

extern "C" {
// 定义函数指针
#define XX(name) name##_fun name##_f = nullptr;
HOOK_FUN(XX)
#undef XX

/**
         * @brief 重写的sleep函数，支持协程调度
         * @param seconds 要休眠的秒数
         * @return 返回0表示成功执行
         *
         * 当hook启用时，该函数会使用IOManager的定时器功能实现非阻塞式休眠：
         * 1. 获取当前IOManager实例
         * 2. 添加一个定时器，在指定时间后触发
         * 3. 定时器回调中将当前协程加入调度队列
         * 4. 让出当前协程的执行权
         * 5. 时间到达后协程会被重新调度执行
         */
unsigned int sleep(unsigned int seconds) {
    if (!is_hook_enable()) {
        return sleep_f(seconds);
    }

    IOManager* io_manager = IOManager::GetThis();
    if (!io_manager) {
        return sleep_f(seconds);
    }

    Coroutine::ptr co = Coroutine::GetThis();
    io_manager->addTimer(seconds * 1000, [io_manager, co]() { io_manager->schedule(co); });
    Coroutine::YieldToHold();

    return 0;
}

/**
         * @brief 重写的usleep函数，支持协程调度
         * @param usec 要休眠的微秒数
         * @return 返回0表示成功执行
         *
         * 当hook启用时，该函数会使用IOManager的定时器功能实现非阻塞式休眠：
         * 1. 获取当前IOManager实例
         * 2. 添加一个定时器，在指定时间后触发
         * 3. 定时器回调中将当前协程加入调度队列
         * 4. 让出当前协程的执行权
         * 5. 时间到达后协程会被重新调度执行
         */
int usleep(useconds_t usec) {
    if (!is_hook_enable()) {
        return usleep_f(usec);
    }

    IOManager* io_manager = IOManager::GetThis();
    if (!io_manager) {
        return usleep_f(usec);
    }

    Coroutine::ptr co = Coroutine::GetThis();
    io_manager->addTimer(usec / 1000, [io_manager, co]() { io_manager->schedule(co); });
    Coroutine::YieldToHold();

    return 0;
}

/**
         * @brief 重写的nanosleep函数，支持协程调度
         * @param req 要休眠的时间间隔
         * @param rem 如果函数被信号中断，返回剩余的时间间隔
         * @return 成功返回0，出错返回-1，并设置errno
         *
         * 当hook启用时，该函数会使用IOManager的定时器功能实现非阻塞式休眠：
         * 1. 获取当前IOManager实例
         * 2. 将timespec结构转换为毫秒数
         * 3. 添加一个定时器，在指定时间后触发
         * 4. 定时器回调中将当前协程加入调度队列
         * 5. 让出当前协程的执行权
         * 6. 时间到达后协程会被重新调度执行
         */
int nanosleep(const struct timespec* req, struct timespec* rem) {
    if (!is_hook_enable()) {
        return nanosleep_f(req, rem);
    }

    IOManager* io_manager = IOManager::GetThis();
    if (!io_manager) {
        return nanosleep_f(req, rem);
    }

    Coroutine::ptr co = Coroutine::GetThis();
    // 将纳秒转换为毫秒
    uint64_t timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 / 1000;
    io_manager->addTimer(timeout_ms, [io_manager, co]() { io_manager->schedule(co); });
    Coroutine::YieldToHold();

    return 0;
}

/**
         * @brief 创建socket并将其纳入hook系统管理
         * @param[in] domain 协议域，决定socket的地址类型，如AF_INET、AF_UNIX等
         * @param[in] type socket类型，如SOCK_STREAM、SOCK_DGRAM等
         * @param[in] protocol 具体协议，通常设为0使用默认协议
         * @return 成功时返回文件描述符，失败时返回-1并设置errno
         */
int socket(int domain, int type, int protocol) {
    // 未开启 hook 则使用系统调用
    if (!is_hook_enable()) {
        return socket_f(domain, type, protocol);
    }

    // 调用系统调用，创建套接字
    int fd = socket_f(domain, type, protocol);
    if (-1 == fd) {
        return fd;
    }

    // 将 fd 注册到FdManager中进行统一管理
    FdMgr::GetInstance()->get(fd, true);
    return fd;
}

/**
         * @brief 带超时的连接函数
         * @param[in] fd 文件描述符
         * @param[in] addr 目标地址结构体指针
         * @param[in] addrlen 地址结构体长度
         * @param[in] timeout_ms 超时时间(毫秒)，-1表示无限等待
         * @return 连接成功返回0，失败返回-1，并设置相应的errno
         *
         * @details 该函数在原始connect基础上增加了超时控制功能。
         *          如果hook未启用、文件描述符不是socket或设置了用户非阻塞模式，则直接调用原始connect函数。
         *          否则通过IOManager实现异步连接，在指定超时时间内等待连接结果。
         */
int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen,
                         uint64_t timeout_ms) {
    // 如果hook未启用，直接调用原始connect函数
    if (!t_hook_enable) {
        return connect_f(fd, addr, addrlen);
    }

    // 获取文件描述符上下文
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(fd);
    if (!ctx || ctx->isClose()) {
        errno = EBADF;
        return -1;
    }

    // 如果不是socket类型，直接调用原始connect函数
    if (!ctx->isSocket()) {
        return connect_f(fd, addr, addrlen);
    }

    // 如果用户设置了非阻塞模式，直接调用原始connect函数
    if (ctx->getUserNonBlock()) {
        return connect_f(fd, addr, addrlen);
    }

    // 尝试连接
    int n = connect_f(fd, addr, addrlen);
    if (n == 0) {
        // 连接立即成功
        return 0;
    } else if (n != -1 || errno != EINPROGRESS) {
        // 连接出现错误或其他情况
        return n;
    }

    // 获取当前IO管理器
    IOManager* iom = IOManager::GetThis();
    Timer::ptr timer;
    std::shared_ptr<timer_info> tinfo(new timer_info);
    std::weak_ptr<timer_info> winfo(tinfo);

    // 如果设置了超时时间，则添加超时定时器，如果在规定时间内未完成连接，则设置错误表示，以返回连接错误
    if (timeout_ms != (uint64_t)-1) {
        timer = iom->addConditionTimer(
            timeout_ms,
            [winfo, fd, iom]() {
                auto t = winfo.lock();
                if (!t || t->cancelled) {
                    return;
                }
                // 设置错误标识
                t->cancelled = ETIMEDOUT;
                // 取消监听事件
                iom->cancelEvent(fd, IOManager::WRITE);
            },
            winfo);
    }

    // 添加写事件监听（连接完成时socket可写）
    bool rt = iom->addEvent(fd, IOManager::WRITE);
    if (!rt) {
        // 事件添加失败，取消定时器并记录日志
        if (timer) {
            timer->cancel();
        }
        CIM_LOG_ERROR(g_logger) << "connect addEvent(" << fd << ", WRITE) error";
    } else {
        // 事件添加成功，让出协程控制权
        Coroutine::YieldToHold();

        // 取消定时器
        if (timer) {
            timer->cancel();
        }

        // 检查是否因超时取消
        if (tinfo->cancelled) {
            errno = tinfo->cancelled;
            return -1;
        }
    }

    // 检查socket错误状态
    int error = 0;
    socklen_t len = sizeof(int);
    if (-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)) {
        return -1;
    }

    // 根据错误状态返回结果
    if (!error) {
        return 0;
    } else {
        errno = error;
        return -1;
    }
}

/**
         * @brief 连接指定的socket地址
         * @param[in] sockfd socket文件描述符
         * @param[in] addr 目标地址结构体指针
         * @param[in] addrlen 地址结构体长度
         * @return 连接成功返回0，失败返回-1，并设置相应的errno
         *
         * @details 该函数是对系统connect函数的hook封装，增加了超时控制功能。
         *          实际调用connect_with_timeout函数实现连接，超时时间使用全局配置的tcp连接超时时间。
         */
int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    return connect_with_timeout(sockfd, addr, addrlen, s_connect_timeout);
}

/**
         * @brief 重写的accept函数，支持协程调度
         * @param[in] sockfd 监听套接字文件描述符
         * @param[out] addr 返回连接方的地址信息
         * @param[out] addrlen 返回地址结构体的长度
         * @return 成功返回新的连接套接字文件描述符，失败返回-1，并设置相应的errno
         *
         * @details 该函数是对系统accept函数的hook封装，具有以下特点：
         *          1. 使用do_io函数处理IO操作，在阻塞时让出协程控制权
         *          2. 对于新建立的连接套接字，通过FdMgr管理其上下文信息
         *          3. 支持超时控制，超时时间由SO_RCVTIMEO选项决定
         */
int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    int fd = do_io(sockfd, accept_f, "accept", IOManager::READ, SO_RCVTIMEO, addr, addrlen);
    if (fd >= 0) {
        FdMgr::GetInstance()->get(fd, true);
    }
    return fd;
}

/**
         * @brief 重写的read函数，支持协程调度
         * @param[in] fd 文件描述符
         * @param[out] buf 数据缓冲区
         * @param[in] count 要读取的字节数
         * @return 成功返回实际读取的字节数，失败返回-1并设置errno
         *
         * @details 使用do_io模板函数处理实际的读操作，在阻塞时能够自动让出协程控制权。
         *          超时时间由文件描述符的SO_RCVTIMEO选项决定。
         */
ssize_t read(int fd, void* buf, size_t count) {
    return do_io(fd, read_f, "read", IOManager::READ, SO_RCVTIMEO, buf, count);
}

/**
         * @brief 重写的readv函数，支持协程调度
         * @param[in] fd 文件描述符
         * @param[in] iov 分散/聚集I/O向量
         * @param[in] iovcnt 向量元素个数
         * @return 成功返回实际读取的字节数，失败返回-1并设置errno
         *
         * @details 使用do_io模板函数处理实际的分散读操作，在阻塞时能够自动让出协程控制权。
         *          超时时间由文件描述符的SO_RCVTIMEO选项决定。
         */
ssize_t readv(int fd, const struct iovec* iov, int iovcnt) {
    return do_io(fd, readv_f, "readv", IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
}

/**
         * @brief 重写的recv函数，支持协程调度
         * @param[in] sockfd socket文件描述符
         * @param[out] buf 数据缓冲区
         * @param[in] len 缓冲区长度
         * @param[in] flags 标志位
         * @return 成功返回实际接收的字节数，失败返回-1并设置errno
         *
         * @details 使用do_io模板函数处理实际的socket接收操作，在阻塞时能够自动让出协程控制权。
         *          超时时间由文件描述符的SO_RCVTIMEO选项决定。
         */
ssize_t recv(int sockfd, void* buf, size_t len, int flags) {
    return do_io(sockfd, recv_f, "recv", IOManager::READ, SO_RCVTIMEO, buf, len, flags);
}

/**
         * @brief 重写的recvfrom函数，支持协程调度
         * @param[in] sockfd socket文件描述符
         * @param[out] buf 数据缓冲区
         * @param[in] len 缓冲区长度
         * @param[in] flags 标志位
         * @param[out] src_addr 发送方地址信息
         * @param[out] addrlen 地址信息长度
         * @return 成功返回实际接收的字节数，失败返回-1并设置errno
         *
         * @details 使用do_io模板函数处理实际的带地址信息的socket接收操作，
         *          在阻塞时能够自动让出协程控制权。超时时间由文件描述符的SO_RCVTIMEO选项决定。
         */
ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr* src_addr,
                 socklen_t* addrlen) {
    return do_io(sockfd, recvfrom_f, "recvfrom", IOManager::READ, SO_RCVTIMEO, buf, len, flags,
                 src_addr, addrlen);
}

/**
         * @brief 重写的recvmsg函数，支持协程调度
         * @param[in] sockfd socket文件描述符
         * @param[out] msg 消息头结构体
         * @param[in] flags 标志位
         * @return 成功返回实际接收的字节数，失败返回-1并设置errno
         *
         * @details 使用do_io模板函数处理实际的消息接收操作，在阻塞时能够自动让出协程控制权。
         *          超时时间由文件描述符的SO_RCVTIMEO选项决定。
         */
ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags) {
    return do_io(sockfd, recvmsg_f, "recvmsg", IOManager::READ, SO_RCVTIMEO, msg, flags);
}

/**
         * @brief 重写的write函数，支持协程调度
         * @param[in] fd 文件描述符
         * @param[in] buf 要写入的数据缓冲区
         * @param[in] count 要写入的字节数
         * @return 成功返回实际写入的字节数，失败返回-1并设置errno
         *
         * @details 使用do_io模板函数处理实际的写操作，在阻塞时能够自动让出协程控制权。
         *          超时时间由文件描述符的SO_SNDTIMEO选项决定。
         */
ssize_t write(int fd, const void* buf, size_t count) {
    return do_io(fd, write_f, "write", IOManager::WRITE, SO_SNDTIMEO, buf, count);
}

/**
         * @brief 重写的writev函数，支持协程调度
         * @param[in] fd 文件描述符
         * @param[in] iov 分散/聚集I/O向量
         * @param[in] iovcnt 向量元素个数
         * @return 成功返回实际写入的字节数，失败返回-1并设置errno
         *
         * @details 使用do_io模板函数处理实际的分散写操作，在阻塞时能够自动让出协程控制权。
         *          超时时间由文件描述符的SO_SNDTIMEO选项决定。
         */
ssize_t writev(int fd, const struct iovec* iov, int iovcnt) {
    return do_io(fd, writev_f, "writev", IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
}

/**
         * @brief 重写的send函数，支持协程调度
         * @param[in] sockfd socket文件描述符
         * @param[in] buf 要发送的数据缓冲区
         * @param[in] len 缓冲区长度
         * @param[in] flags 标志位
         * @return 成功返回实际发送的字节数，失败返回-1并设置errno
         *
         * @details 使用do_io模板函数处理实际的socket发送操作，在阻塞时能够自动让出协程控制权。
         *          超时时间由文件描述符的SO_SNDTIMEO选项决定。
         */
ssize_t send(int sockfd, const void* buf, size_t len, int flags) {
    return do_io(sockfd, send_f, "send", IOManager::WRITE, SO_SNDTIMEO, buf, len, flags);
}

/**
         * @brief 重写的sendto函数，支持协程调度
         * @param[in] sockfd socket文件描述符
         * @param[in] buf 要发送的数据缓冲区
         * @param[in] len 缓冲区长度
         * @param[in] flags 标志位
         * @param[in] dest_addr 目标地址信息
         * @param[in] addrlen 地址信息长度
         * @return 成功返回实际发送的字节数，失败返回-1并设置errno
         *
         * @details 使用do_io模板函数处理实际的带地址信息的socket发送操作，
         *          在阻塞时能够自动让出协程控制权。超时时间由文件描述符的SO_SNDTIMEO选项决定。
         */
ssize_t sendto(int sockfd, const void* buf, size_t len, int flags, const struct sockaddr* dest_addr,
               socklen_t addrlen) {
    return do_io(sockfd, sendto_f, "sendto", IOManager::WRITE, SO_SNDTIMEO, buf, len, flags,
                 dest_addr, addrlen);
}

/**
         * @brief 重写的sendmsg函数，支持协程调度
         * @param[in] sockfd socket文件描述符
         * @param[in] msg 消息头结构体
         * @param[in] flags 标志位
         * @return 成功返回实际发送的字节数，失败返回-1并设置errno
         *
         * @details 使用do_io模板函数处理实际的消息发送操作，在阻塞时能够自动让出协程控制权。
         *          超时时间由文件描述符的SO_SNDTIMEO选项决定。
         */
ssize_t sendmsg(int sockfd, const struct msghdr* msg, int flags) {
    return do_io(sockfd, sendmsg_f, "sendmsg", IOManager::WRITE, SO_SNDTIMEO, msg, flags);
}

/**
         * @brief 关闭文件描述符
         * @param[in] fd 需要关闭的文件描述符
         * @return int 成功返回0，失败返回-1并设置errno
         *
         * @details 该函数是系统close函数的hook版本，在关闭文件描述符前会先清理相关的IO事件监听。
         *          如果hook功能未启用，则直接调用原始的close函数。
         *          如果文件描述符存在上下文管理对象，则会取消该fd上的所有IO事件监听并删除上下文。
         */
int close(int fd) {
    if (!is_hook_enable()) {
        return close_f(fd);
    }

    FdCtx::ptr ctx = FdMgr::GetInstance()->get(fd);
    if (ctx) {
        auto iom = IOManager::GetThis();
        if (iom) {
            // 取消该文件描述符上所有IO事件监听
            iom->cancelAll(fd);
        }
        // 从管理器中删除该文件描述符的上下文
        FdMgr::GetInstance()->del(fd);
    }
    // 调用原始的close函数关闭文件描述符
    return close_f(fd);
}

/**
         * @brief 重写的fcntl函数，支持协程调度
         * @param[in] fd 文件描述符
         * @param[in] cmd 控制命令
         * @param[in] ... 可变参数，根据cmd的不同而不同
         * @return 根据cmd的不同，返回不同的值
         *
         * @details 该函数是对系统fcntl函数的hook封装，主要处理与非阻塞I/O相关的标志位。
         *          对于socket类型的文件描述符，会区分系统设置的非阻塞标记和用户设置的非阻塞标记，
         *          确保在协程环境中正确处理阻塞/非阻塞行为。
         *
         *          支持的主要命令包括：
         *          - F_SETFL: 设置文件状态标志
         *          - F_GETFL: 获取文件状态标志
         *          - F_DUPFD, F_DUPFD_CLOEXEC等其他命令直接转发给系统调用
         */
int fcntl(int fd, int cmd, ...) {
    va_list args;
    va_start(args, cmd);
    switch (cmd) {
        // 设置文件状态标志
        case F_SETFL: {
            int arg = va_arg(args, int);
            va_end(args);
            FdCtx::ptr ctx = FdMgr::GetInstance()->get(fd);
            if (!ctx || !ctx->isSocket() || ctx->isClose()) {
                return fcntl_f(fd, cmd, arg);
            }
            ctx->setUserNonBlock(arg & O_NONBLOCK);
            if (ctx->getSysNonBlock()) {
                arg |= O_NONBLOCK;
            } else {
                arg &= ~O_NONBLOCK;
            }
            return fcntl_f(fd, cmd, arg);
        } break;
        // 获取文件状态标志
        case F_GETFL: {
            va_end(args);
            int arg = fcntl_f(fd, cmd);
            FdCtx::ptr ctx = FdMgr::GetInstance()->get(fd);
            if (!ctx || !ctx->isSocket() || ctx->isClose()) {
                return arg;
            }
            if (ctx->getUserNonBlock()) {
                return arg | O_NONBLOCK;
            } else {
                return arg & ~O_NONBLOCK;
            }
        } break;
        // 处理需要一个整型参数的命令，直接转发给系统调用
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
        case F_SETPIPE_SZ: {
            int arg = va_arg(args, int);
            va_end(args);
            return fcntl_f(fd, cmd, arg);
        } break;

        // 处理不需要额外参数的命令，直接转发给系统调用
        case F_GETFD:

        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
        case F_GETPIPE_SZ: {
            va_end(args);
            return fcntl_f(fd, cmd);
        } break;

        // 处理文件锁定相关命令，直接转发给系统调用
        case F_SETLK:
        case F_SETLKW:
        case F_GETLK: {
            struct flock* arg = va_arg(args, struct flock*);
            va_end(args);
            return fcntl_f(fd, cmd, arg);
        } break;

        // 处理文件所有者扩展结构命令，直接转发给系统调用
        case F_GETOWN_EX:
        case F_SETOWN_EX: {
            struct f_owner_exlock* arg = va_arg(args, struct f_owner_exlock*);
            va_end(args);
            return fcntl_f(fd, cmd, arg);
        } break;
        // 其他未处理的命令，直接转发给系统调用
        default: {
            va_end(args);
            return fcntl_f(fd, cmd);
        }
    }
}

/**
         * @brief ioctl系统调用的hook函数，用于控制套接字的I/O模式和其他属性
         * @param[in] fd 文件描述符
         * @param[in] request 操作命令，如FIONBIO等
         * @param[in] ... 可变参数，根据request的不同而不同
         * @return 成功返回0，失败返回-1并设置errno
         *
         * @details 该函数主要处理FIONBIO命令，用于设置或清除套接字的非阻塞I/O模式。
         *          对于其他命令，直接转发给系统原始的ioctl函数处理。
         */
int ioctl(int fd, unsigned long request, ...) {
    va_list args;
    va_start(args, request);
    void* arg = va_arg(args, void*);
    va_end(args);

    // 处理FIONBIO命令，设置或清除套接字的非阻塞I/O模式
    if (FIONBIO == request) {
        bool user_nonblock = !!(*(int*)arg);
        FdCtx::ptr ctx = FdMgr::GetInstance()->get(fd);
        if (!ctx || !ctx->isSocket() || ctx->isClose()) {
            return ioctl_f(fd, request, arg);
        }
        ctx->setSysNonBlock(user_nonblock);
    }
    return ioctl_f(fd, request, arg);
}
}

/**
     * @brief 重写的getsockopt函数
     * @param[in] sockfd socket文件描述符
     * @param[in] level 选项定义层次
     * @param[in] optname 需要查询的选项名
     * @param[out] optval 用于存放选项值的缓冲区
     * @param[in,out] optlen optval缓冲区长度，返回时为实际数据长度
     * @return 成功返回0，失败返回-1并设置errno
     *
     * @details 该函数目前直接转发给系统原始函数，未做特殊处理。
     */
int getsockopt(int sockfd, int level, int optname, void* optval, socklen_t* optlen) {
    return getsockopt_f(sockfd, level, optname, optval, optlen);
}

/**
     * @brief 重写的setsockopt函数
     * @param[in] sockfd socket文件描述符
     * @param[in] level 选项定义层次
     * @param[in] optname 需要设置的选项名
     * @param[in] optval 包含选项值的缓冲区
     * @param[in] optlen optval缓冲区长度
     * @return 成功返回0，失败返回-1并设置errno
     *
     * @details 该函数在原始setsockopt基础上增加了对超时选项的处理。
     *          当设置SO_RCVTIMEO或SO_SNDTIMEO选项时，会同时更新FdCtx中的超时设置。
     */
int setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen) {
    if (!is_hook_enable()) {
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }

    if (level == SOL_SOCKET) {
        if (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
            FdCtx::ptr ctx = FdMgr::GetInstance()->get(sockfd);
            if (ctx) {
                // 将传入的timeval结构体转换为毫秒数，并设置到上下文中的超时时间
                const timeval* tv = (const timeval*)optval;
                ctx->setTimeout(optname, tv->tv_sec * 1000 + tv->tv_usec / 1000);
            }
        }
    }
    return setsockopt_f(sockfd, level, optname, optval, optlen);
}
}  // namespace CIM