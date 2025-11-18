#include "net/fd_manager.hpp"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "net/hook.hpp"

namespace CIM {
FdCtx::FdCtx(int fd)
    : m_isInit(false),
      m_isSocket(false),
      m_sysNonBlock(false),
      m_userNonBlock(false),
      m_isClosed(false),
      m_fd(fd),
      m_recvTimeout(-1),
      m_sendTimeout(-1) {
    init();
}

/**
     * @brief 初始化文件描述符上下文
     * @details 检查文件描述符的有效性，判断是否为socket，
     *          并设置非阻塞模式等相关属性
     * @return 初始化成功返回true，失败返回false
     *
     * 1.检查文件描述符是否已经初始化
     * 2.设置接收和发送超时时间为-1（无限等待）
     * 3.使用fstat检查文件描述符状态，并判断是否为socket类型
     * 4.如果文件描述符是socket，则设置非阻塞模式
     * 5.初始化各种标志位
     */
bool FdCtx::init() {
    // 检查文件描述符是否已经初始化
    if (m_isInit) {
        return true;
    }

    // 设置接收和发送超时为-1（无限等待）
    m_recvTimeout = -1;
    m_sendTimeout = -1;

    // 使用fstat检查文件描述符状态
    struct stat fd_stat;
    if (-1 == fstat(m_fd, &fd_stat)) {
        m_isInit = false;
        m_isSocket = false;
    } else {
        m_isInit = true;
        m_isSocket = S_ISSOCK(fd_stat.st_mode);  // 判断是否为socket类型
    }

    // 检查文件描述符是否为非阻塞模式，如果不是，则设置为非阻塞模式
    if (m_isSocket) {
        int flags = fcntl_f(m_fd, F_GETFL, 0);
        if (!(flags & O_NONBLOCK)) {
            fcntl_f(m_fd, F_SETFL, flags | O_NONBLOCK);  // 设置非阻塞模式
        }
        m_sysNonBlock = true;
    } else {
        m_sysNonBlock = false;
    }

    m_userNonBlock = false;
    m_isClosed = false;
    return m_isInit;
}

bool FdCtx::isInit() const {
    return m_isInit;
}

bool FdCtx::isSocket() const {
    return m_isSocket;
}

bool FdCtx::isClose() const {
    return m_isClosed;
}

bool FdCtx::close() {
    return false;
}

void FdCtx::setUserNonBlock(bool v) {
    m_userNonBlock = v;
}

bool FdCtx::getUserNonBlock() const {
    return m_userNonBlock;
}

void FdCtx::setSysNonBlock(bool v) {
    m_sysNonBlock = v;
}

bool FdCtx::getSysNonBlock() const {
    return m_sysNonBlock;
}

void FdCtx::setTimeout(int type, uint64_t v) {
    if (type == SO_RCVTIMEO) {
        m_recvTimeout = v;
    } else if (type == SO_SNDTIMEO) {
        m_sendTimeout = v;
    }
}

uint64_t FdCtx::getTimeout(int type) const {
    if (type == SO_RCVTIMEO) {
        return m_recvTimeout;
    } else if (type == SO_SNDTIMEO) {
        return m_sendTimeout;
    }
    return -1;
}

FdManager::FdManager() {
    m_fdCtxs.resize(64);
}

FdCtx::ptr FdManager::get(int fd, bool auto_create) {
    if (fd < 0) {
        return nullptr;
    }

    RWMutexType::WriteLock lock(m_mutex);
    if (fd >= (int)m_fdCtxs.size()) {
        if (!auto_create) {
            return nullptr;
        }
        m_fdCtxs.resize(fd * 1.5 + 1);  // 防止 fd 是0或1
    } else {
        if (m_fdCtxs[fd]) {
            return m_fdCtxs[fd];
        }
        if (!auto_create) {
            return nullptr;
        }
    }

    FdCtx::ptr ctx = std::make_shared<FdCtx>(fd);
    m_fdCtxs[fd] = ctx;
    return ctx;
}

void FdManager::del(int fd) {
    RWMutexType::WriteLock lock(m_mutex);
    if (fd < 0 || fd >= (int)m_fdCtxs.size()) {
        return;
    }
    m_fdCtxs[fd].reset();
}

FileDescriptor::FileDescriptor(int fd) : m_fd(fd) {}

FileDescriptor::~FileDescriptor() {
    if (m_fd != -1) {
        close(m_fd);
    }
}

int FileDescriptor::get() const {
    return m_fd;
}

void FileDescriptor::reset(int fd) {
    if (m_fd != -1) {
        close(m_fd);
    }
    m_fd = fd;
}

int FileDescriptor::release() {
    int fd = m_fd;
    m_fd = -1;
    return fd;
}

bool FileDescriptor::isValid() const {
    return m_fd != -1;
}

FileDescriptor::FileDescriptor(FileDescriptor&& other) noexcept : m_fd(other.release()) {}

FileDescriptor& FileDescriptor::operator=(FileDescriptor&& other) noexcept {
    if (this != &other) {
        reset(other.release());
    }
    return *this;
}
}  // namespace CIM