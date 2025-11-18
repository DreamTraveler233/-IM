#include "net/socket.hpp"

#include <limits.h>

#include "net/fd_manager.hpp"
#include "net/hook.hpp"
#include "io/iomanager.hpp"
#include "base/macro.hpp"

namespace CIM {
static auto g_logger = CIM_LOG_NAME("system");

Socket::ptr Socket::CreateTCP(CIM::Address::ptr address) {
    Socket::ptr sock(new Socket(address->getFamily(), TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDP(CIM::Address::ptr address) {
    Socket::ptr sock(new Socket(address->getFamily(), UDP, 0));
    sock->newSock();
    sock->m_isConnected = true;
    return sock;
}

Socket::ptr Socket::CreateTCPSocket() {
    Socket::ptr sock(new Socket(IPv4, TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDPSocket() {
    Socket::ptr sock(new Socket(IPv4, UDP, 0));
    sock->newSock();
    sock->m_isConnected = true;
    return sock;
}

Socket::ptr Socket::CreateTCPSocket6() {
    Socket::ptr sock(new Socket(IPv6, TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDPSocket6() {
    Socket::ptr sock(new Socket(IPv6, UDP, 0));
    sock->newSock();
    sock->m_isConnected = true;
    return sock;
}

Socket::ptr Socket::CreateUnixTCPSocket() {
    Socket::ptr sock(new Socket(UNIX, TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUnixUDPSocket() {
    Socket::ptr sock(new Socket(UNIX, UDP, 0));
    return sock;
}

Socket::Socket(int family, int type, int protocol)
    : m_sock(-1), m_family(family), m_type(type), m_protocol(protocol), m_isConnected(false) {}

Socket::~Socket() {
    close();
}

int64_t Socket::getSendTimeout() {
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(m_sock);
    if (ctx) {
        return ctx->getTimeout(SO_SNDTIMEO);
    }
    return -1;
}

void Socket::setSendTimeout(int64_t v) {
    struct timeval tv{int(v / 1000), int(v % 1000 * 1000)};
    setOption(SOL_SOCKET, SO_SNDTIMEO, tv);
}

int64_t Socket::getRecvTimeout() {
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(m_sock);
    if (ctx) {
        return ctx->getTimeout(SO_RCVTIMEO);
    }
    return -1;
}

void Socket::setRecvTimeout(int64_t v) {
    struct timeval tv{int(v / 1000), int(v % 1000 * 1000)};
    setOption(SOL_SOCKET, SO_RCVTIMEO, tv);
}

bool Socket::getOption(int level, int option, void* result, socklen_t* len) {
    int rt = getsockopt(m_sock, level, option, result, (socklen_t*)len);
    if (rt) {
        CIM_LOG_DEBUG(g_logger) << "getOption sock=" << m_sock << " level=" << level
                                << " option=" << option << " errno=" << errno
                                << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

bool Socket::setOption(int level, int option, const void* result, socklen_t len) {
    if (setsockopt(m_sock, level, option, result, (socklen_t)len)) {
        CIM_LOG_DEBUG(g_logger) << "setOption sock=" << m_sock << " level=" << level
                                << " option=" << option << " errno=" << errno
                                << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

Socket::ptr Socket::accept() {
    // 创建套接字对象，此时 sock.m_fd = -1
    Socket::ptr sock(new Socket(m_family, m_type, m_protocol));

    // 接受新连接，使用 hook 后的 accept
    int newsock = ::accept(m_sock, nullptr, nullptr);
    if (newsock == -1) {
        CIM_LOG_ERROR(g_logger) << "accept(" << m_sock << ") errno=" << errno
                                << " errstr=" << strerror(errno);
        return nullptr;
    }

    // 初始化套接字，将 newsock 用 Socket 封装
    if (sock->init(newsock)) {
        return sock;
    }
    return nullptr;
}

bool Socket::bind(const Address::ptr addr) {
    if (!isValid()) {
        newSock();
        if (CIM_UNLIKELY(!isValid())) {
            return false;
        }
    }

    if (CIM_UNLIKELY(addr->getFamily() != m_family)) {
        CIM_LOG_ERROR(g_logger) << "bind sock.family(" << m_family << ") addr.family("
                                << addr->getFamily() << ") not equal, addr=" << addr->toString();
        return false;
    }

    // Unix域地址特殊处理
    UnixAddress::ptr uaddr = std::dynamic_pointer_cast<UnixAddress>(addr);
    if (uaddr) {
        Socket::ptr sock = Socket::CreateUnixTCPSocket();
        if (sock->connect(uaddr)) {
            return false;
        } else {
            CIM::FSUtil::Unlink(uaddr->getPath(), true);
        }
    }

    // 绑定地址
    if (::bind(m_sock, addr->getAddr(), addr->getAddrLen())) {
        CIM_LOG_ERROR(g_logger) << "bind error errrno=" << errno << " errstr=" << strerror(errno);
        return false;
    }

    getLocalAddress();
    return true;
}

bool Socket::reconnect(uint64_t timeout_ms) {
    if (!m_remoteAddress) {
        CIM_LOG_ERROR(g_logger) << "reconnect m_remoteAddress is null";
        return false;
    }
    m_localAddress.reset();
    return connect(m_remoteAddress, timeout_ms);
}

bool Socket::connect(const Address::ptr addr, uint64_t timeout_ms) {
    m_remoteAddress = addr;
    if (!isValid()) {
        newSock();
        if (CIM_UNLIKELY(!isValid())) {
            return false;
        }
    }

    if (CIM_UNLIKELY(addr->getFamily() != m_family)) {
        CIM_LOG_ERROR(g_logger) << "connect sock.family(" << m_family << ") addr.family("
                                << addr->getFamily() << ") not equal, addr=" << addr->toString();
        return false;
    }

    if (timeout_ms == (uint64_t)-1) {
        if (::connect(m_sock, addr->getAddr(), addr->getAddrLen())) {
            CIM_LOG_ERROR(g_logger) << "sock=" << m_sock << " connect(" << addr->toString()
                                    << ") error errno=" << errno << " errstr=" << strerror(errno);
            close();
            return false;
        }
    } else {
        if (::connect_with_timeout(m_sock, addr->getAddr(), addr->getAddrLen(), timeout_ms)) {
            CIM_LOG_ERROR(g_logger) << "sock=" << m_sock << " connect(" << addr->toString()
                                    << ") timeout=" << timeout_ms << " error errno=" << errno
                                    << " errstr=" << strerror(errno);
            close();
            return false;
        }
    }
    m_isConnected = true;
    getRemoteAddress();
    getLocalAddress();
    return true;
}

bool Socket::listen(int backlog) {
    if (!isValid()) {
        CIM_LOG_ERROR(g_logger) << "listen error sock=-1";
        return false;
    }
    if (::listen(m_sock, backlog)) {
        CIM_LOG_ERROR(g_logger) << "listen error errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

bool Socket::init(int sock) {
    // 在fd管理器中注册该套接字，方便统一管理
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(sock);
    if (ctx && ctx->isSocket() && !ctx->isClose()) {
        // 初始化成员变量
        m_sock = sock;
        m_isConnected = true;
        // 设置默认选项
        setDefaultOptions();
        // 设置服务器地址
        getLocalAddress();
        // 设置客户端地址
        getRemoteAddress();
        return true;
    }
    return false;
}

void Socket::setDefaultOptions() {
    int val = 1;
    // 设置 SO_REUSEADDR 允许地址重用
    setOption(SOL_SOCKET, SO_REUSEADDR, val);

    // 如果是 TCP 协议，禁止 Nagle 算法
    if (m_type == SOCK_STREAM) {
        setOption(IPPROTO_TCP, TCP_NODELAY, val);
    }
}

Address::ptr Socket::getRemoteAddress() {
    if (m_remoteAddress) {
        return m_remoteAddress;
    }

    // 创建对应协议的地址
    Address::ptr result;
    switch (m_family) {
        case AF_INET:  // IPv4
            result.reset(new IPv4Address());
            break;
        case AF_INET6:  // IPv6
            result.reset(new IPv6Address());
            break;
        case AF_UNIX:  // Unix
            result.reset(new UnixAddress());
            break;
        default:
            result.reset(new UnknownAddress(m_family));
            break;
    }

    // 获取远端地址
    socklen_t addrlen = result->getAddrLen();
    if (getpeername(m_sock, result->getAddr(), &addrlen)) {
        CIM_LOG_ERROR(g_logger) << "getpeername error sock=" << m_sock << " errno=" << errno
                                << " errstr=" << strerror(errno);
        return Address::ptr(new UnknownAddress(m_family));
    }

    // 特殊处理
    if (m_family == AF_UNIX) {
        UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
        addr->setAddrLen(addrlen);
    }

    m_remoteAddress = result;
    return m_remoteAddress;
}

Address::ptr Socket::getLocalAddress() {
    if (m_localAddress) {
        return m_localAddress;
    }

    // 创建对应协议的地址
    Address::ptr result;
    switch (m_family) {
        case AF_INET:  // IPv4
            result.reset(new IPv4Address());
            break;
        case AF_INET6:  // IPv6
            result.reset(new IPv6Address());
            break;
        case AF_UNIX:  // Unix
            result.reset(new UnixAddress());
            break;
        default:
            result.reset(new UnknownAddress(m_family));
            break;
    }

    // 获取本地地址
    socklen_t addrlen = result->getAddrLen();
    if (getsockname(m_sock, result->getAddr(), &addrlen)) {
        CIM_LOG_ERROR(g_logger) << "getsockname error sock=" << m_sock << " errno=" << errno
                                << " errstr=" << strerror(errno);
        return Address::ptr(new UnknownAddress(m_family));
    }

    // 特殊处理
    if (m_family == AF_UNIX) {
        UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
        addr->setAddrLen(addrlen);
    }

    m_localAddress = result;
    return m_localAddress;
}

bool Socket::close() {
    if (!m_isConnected && m_sock == -1) {
        return true;
    }
    m_isConnected = false;
    if (m_sock != -1) {
        ::close(m_sock);
        m_sock = -1;
    }
    return false;
}

int Socket::send(const void* buffer, size_t length, int flags) {
    if (isConnected()) {
        return ::send(m_sock, buffer, length, flags);
    }
    return -1;
}

int Socket::send(const iovec* buffers, size_t length, int flags) {
    if (isConnected()) {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;
        return ::sendmsg(m_sock, &msg, flags);
    }
    return -1;
}

int Socket::sendTo(const void* buffer, size_t length, const Address::ptr to, int flags) {
    if (isConnected()) {
        return ::sendto(m_sock, buffer, length, flags, to->getAddr(), to->getAddrLen());
    }
    return -1;
}

int Socket::sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags) {
    if (isConnected()) {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;
        msg.msg_name = to->getAddr();
        msg.msg_namelen = to->getAddrLen();
        return ::sendmsg(m_sock, &msg, flags);
    }
    return -1;
}

int Socket::recv(void* buffer, size_t length, int flags) {
    if (isConnected()) {
        return ::recv(m_sock, buffer, length, flags);
    }
    return -1;
}

int Socket::recv(iovec* buffers, size_t length, int flags) {
    if (isConnected()) {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;
        return ::recvmsg(m_sock, &msg, flags);
    }
    return -1;
}

int Socket::recvFrom(void* buffer, size_t length, Address::ptr from, int flags) {
    if (isConnected()) {
        socklen_t len = from->getAddrLen();
        return ::recvfrom(m_sock, buffer, length, flags, from->getAddr(), &len);
    }
    return -1;
}

int Socket::recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags) {
    if (isConnected()) {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;
        msg.msg_name = from->getAddr();
        msg.msg_namelen = from->getAddrLen();
        return ::recvmsg(m_sock, &msg, flags);
    }
    return -1;
}

int Socket::getFamily() const {
    return m_family;
}

int Socket::getType() const {
    return m_type;
}

int Socket::getProtocol() const {
    return m_protocol;
}

bool Socket::isConnected() const {
    return m_isConnected;
}

bool Socket::isValid() const {
    return m_sock != -1;
}

int Socket::getError() {
    int error = 0;
    socklen_t len = sizeof(error);
    if (!getOption(SOL_SOCKET, SO_ERROR, &error, &len)) {
        error = errno;
    }
    return error;
}

std::ostream& Socket::dump(std::ostream& os) const {
    os << "[Socket sock=" << m_sock << " is_connected=" << m_isConnected << " family=" << m_family
       << " type=" << m_type << " protocol=" << m_protocol;
    if (m_localAddress) {
        os << " local_address=" << m_localAddress->toString();
    }
    if (m_remoteAddress) {
        os << " remote_address=" << m_remoteAddress->toString();
    }
    os << "]";
    return os;
}

std::string Socket::toString() const {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

int Socket::getSocket() const {
    return m_sock;
}

bool Socket::cancelRead() {
    return IOManager::GetThis()->cancelEvent(m_sock, CIM::IOManager::READ);
}

bool Socket::cancelWrite() {
    return IOManager::GetThis()->cancelEvent(m_sock, CIM::IOManager::WRITE);
}

bool Socket::cancelAccept() {
    return IOManager::GetThis()->cancelEvent(m_sock, CIM::IOManager::READ);
}

bool Socket::cancelAll() {
    return IOManager::GetThis()->cancelAll(m_sock);
}

void Socket::newSock() {
    m_sock = socket(m_family, m_type, m_protocol);
    if (CIM_LIKELY(m_sock != -1)) {
        setDefaultOptions();
    } else {
        CIM_LOG_ERROR(g_logger) << "socket(" << m_family << ", " << m_type << ", " << m_protocol
                                << ") errno=" << errno << " errstr=" << strerror(errno);
    }
}

namespace {

struct _SSLInit {
    _SSLInit() {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();
    }
};

static _SSLInit s_init;

}  // namespace

SSLSocket::SSLSocket(int family, int type, int protocol) : Socket(family, type, protocol) {}

Socket::ptr SSLSocket::accept() {
    SSLSocket::ptr sock(new SSLSocket(m_family, m_type, m_protocol));
    int newsock = ::accept(m_sock, nullptr, nullptr);
    if (newsock == -1) {
        CIM_LOG_ERROR(g_logger) << "accept(" << m_sock << ") errno=" << errno
                                << " errstr=" << strerror(errno);
        return nullptr;
    }
    sock->m_ctx = m_ctx;
    if (sock->init(newsock)) {
        return sock;
    }
    return nullptr;
}

bool SSLSocket::bind(const Address::ptr addr) {
    return Socket::bind(addr);
}

bool SSLSocket::connect(const Address::ptr addr, uint64_t timeout_ms) {
    bool v = Socket::connect(addr, timeout_ms);
    if (v) {
        m_ctx.reset(SSL_CTX_new(SSLv23_client_method()), SSL_CTX_free);
        m_ssl.reset(SSL_new(m_ctx.get()), SSL_free);
        SSL_set_fd(m_ssl.get(), m_sock);
        v = (SSL_connect(m_ssl.get()) == 1);
    }
    return v;
}

bool SSLSocket::listen(int backlog) {
    return Socket::listen(backlog);
}

bool SSLSocket::close() {
    return Socket::close();
}

int SSLSocket::send(const void* buffer, size_t length, int flags) {
    if (m_ssl) {
        return SSL_write(m_ssl.get(), buffer, length);
    }
    return -1;
}

int SSLSocket::send(const iovec* buffers, size_t length, int flags) {
    if (!m_ssl) {
        return -1;
    }
    int total = 0;
    for (size_t i = 0; i < length; ++i) {
        int tmp = SSL_write(m_ssl.get(), buffers[i].iov_base, buffers[i].iov_len);
        if (tmp <= 0) {
            return tmp;
        }
        total += tmp;
        if (tmp != (int)buffers[i].iov_len) {
            break;
        }
    }
    return total;
}

int SSLSocket::sendTo(const void* buffer, size_t length, const Address::ptr to, int flags) {
    CIM_ASSERT(false);
    return -1;
}

int SSLSocket::sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags) {
    CIM_ASSERT(false);
    return -1;
}

int SSLSocket::recv(void* buffer, size_t length, int flags) {
    if (m_ssl) {
        return SSL_read(m_ssl.get(), buffer, length);
    }
    return -1;
}

int SSLSocket::recv(iovec* buffers, size_t length, int flags) {
    if (!m_ssl) {
        return -1;
    }
    int total = 0;
    for (size_t i = 0; i < length; ++i) {
        int tmp = SSL_read(m_ssl.get(), buffers[i].iov_base, buffers[i].iov_len);
        if (tmp <= 0) {
            return tmp;
        }
        total += tmp;
        if (tmp != (int)buffers[i].iov_len) {
            break;
        }
    }
    return total;
}

int SSLSocket::recvFrom(void* buffer, size_t length, Address::ptr from, int flags) {
    CIM_ASSERT(false);
    return -1;
}

int SSLSocket::recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags) {
    CIM_ASSERT(false);
    return -1;
}

bool SSLSocket::init(int sock) {
    bool v = Socket::init(sock);
    if (v) {
        m_ssl.reset(SSL_new(m_ctx.get()), SSL_free);
        SSL_set_fd(m_ssl.get(), m_sock);
        v = (SSL_accept(m_ssl.get()) == 1);
    }
    return v;
}

bool SSLSocket::loadCertificates(const std::string& cert_file, const std::string& key_file) {
    m_ctx.reset(SSL_CTX_new(SSLv23_server_method()), SSL_CTX_free);
    if (SSL_CTX_use_certificate_chain_file(m_ctx.get(), cert_file.c_str()) != 1) {
        CIM_LOG_ERROR(g_logger) << "SSL_CTX_use_certificate_chain_file(" << cert_file << ") error";
        return false;
    }
    if (SSL_CTX_use_PrivateKey_file(m_ctx.get(), key_file.c_str(), SSL_FILETYPE_PEM) != 1) {
        CIM_LOG_ERROR(g_logger) << "SSL_CTX_use_PrivateKey_file(" << key_file << ") error";
        return false;
    }
    if (SSL_CTX_check_private_key(m_ctx.get()) != 1) {
        CIM_LOG_ERROR(g_logger) << "SSL_CTX_check_private_key cert_file=" << cert_file
                                << " key_file=" << key_file;
        return false;
    }
    return true;
}

SSLSocket::ptr SSLSocket::CreateTCP(CIM::Address::ptr address) {
    SSLSocket::ptr sock(new SSLSocket(address->getFamily(), TCP, 0));
    return sock;
}

SSLSocket::ptr SSLSocket::CreateTCPSocket() {
    SSLSocket::ptr sock(new SSLSocket(IPv4, TCP, 0));
    return sock;
}

SSLSocket::ptr SSLSocket::CreateTCPSocket6() {
    SSLSocket::ptr sock(new SSLSocket(IPv6, TCP, 0));
    return sock;
}

std::ostream& SSLSocket::dump(std::ostream& os) const {
    os << "[SSLSocket sock=" << m_sock << " is_connected=" << m_isConnected
       << " family=" << m_family << " type=" << m_type << " protocol=" << m_protocol;
    if (m_localAddress) {
        os << " local_address=" << m_localAddress->toString();
    }
    if (m_remoteAddress) {
        os << " remote_address=" << m_remoteAddress->toString();
    }
    os << "]";
    return os;
}

std::ostream& operator<<(std::ostream& os, const Socket& sock) {
    return sock.dump(os);
}

}  // namespace CIM