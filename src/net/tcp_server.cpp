#include "net/tcp_server.hpp"

#include "config/config.hpp"
#include "base/macro.hpp"

namespace CIM {
static CIM::Logger::ptr g_logger = CIM_LOG_NAME("system");

// 配置 TCP 读超时事件
static auto g_tcp_server_read_timeout = CIM::Config::Lookup(
    "tcp_server.read_timeout", (uint64_t)(60 * 1000 * 2), "tcp server read timeout");

TcpServer::TcpServer(CIM::IOManager* worker, CIM::IOManager* io_worker,
                     CIM::IOManager* accept_worker)
    : m_worker(worker),
      m_ioWorker(io_worker),
      m_acceptWorker(accept_worker),
      m_recvTimeout(g_tcp_server_read_timeout->getValue()),
      m_name("CIM/1.0.0"),
      m_isRun(false) {}

TcpServer::~TcpServer() {
    for (auto& i : m_socks) {
        i->close();
    }
    m_socks.clear();
}

void TcpServer::setConf(const TcpServerConf& v) {
    m_conf.reset(new TcpServerConf(v));
}

bool TcpServer::bind(CIM::Address::ptr addr, bool ssl) {
    std::vector<Address::ptr> addrs;
    std::vector<Address::ptr> fails;
    addrs.push_back(addr);
    return bind(addrs, fails, ssl);
}

bool TcpServer::bind(const std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& fails,
                     bool ssl) {
    m_ssl = ssl;
    for (auto& addr : addrs) {
        // 根据是否启用ssl加密创建 Socket
        Socket::ptr sock = ssl ? SSLSocket::CreateTCP(addr) : Socket::CreateTCP(addr);

        // 绑定地址
        if (!sock->bind(addr)) {
            CIM_LOG_ERROR(g_logger) << "bind fail errno=" << errno << " errstr=" << strerror(errno)
                                    << " addr=[" << addr->toString() << "]";
            fails.push_back(addr);
            continue;
        }

        // 开启监听
        if (!sock->listen()) {
            CIM_LOG_ERROR(g_logger)
                << "listen fail errno=" << errno << " errstr=" << strerror(errno) << " addr=["
                << addr->toString() << "]";
            fails.push_back(addr);
            continue;
        }
        m_socks.push_back(sock);
    }

    if (!fails.empty()) {
        m_socks.clear();
        return false;
    }

    for (auto& i : m_socks) {
        CIM_LOG_INFO(g_logger) << "type=" << m_type << " name=" << m_name << " ssl=" << m_ssl
                               << " server bind success: " << *i;
    }
    return true;
}

bool TcpServer::start() {
    if (m_isRun) {
        return false;
    }
    m_isRun = true;

    for (auto& sock : m_socks) {
        m_acceptWorker->schedule(std::bind(&TcpServer::startAccept, shared_from_this(), sock));
    }
    return true;
}

void TcpServer::startAccept(Socket::ptr sock) {
    while (m_isRun) {
        // 接受新连接
        Socket::ptr client_fd = sock->accept();
        if (client_fd) {
            // 设置读超时时间
            client_fd->setRecvTimeout(m_recvTimeout);
            m_ioWorker->schedule(
                std::bind(&TcpServer::handleClient, shared_from_this(), client_fd));
        } else {
            CIM_LOG_ERROR(g_logger) << "accept errno=" << errno << " errstr=" << strerror(errno);
        }
    }
}

void TcpServer::handleClient(Socket::ptr client) {
    CIM_LOG_INFO(g_logger) << "handleClient: " << *client;
}

void TcpServer::stop() {
    m_isRun = false;
    auto self = shared_from_this();
    m_acceptWorker->schedule([this, self]() {
        for (auto& sock : m_socks) {
            sock->cancelAll();
            sock->close();
        }
        m_socks.clear();
    });
}

uint64_t TcpServer::getRecvTimeout() const {
    return m_recvTimeout;
}

std::string TcpServer::getName() const {
    return m_name;
}

void TcpServer::setRecvTimeout(uint64_t v) {
    m_recvTimeout = v;
}

void TcpServer::setName(const std::string& v) {
    m_name = v;
}

bool TcpServer::isRun() const {
    return m_isRun;
}

TcpServerConf::ptr TcpServer::getConf() const {
    return m_conf;
}

void TcpServer::setConf(TcpServerConf::ptr v) {
    m_conf = v;
}

bool TcpServer::loadCertificates(const std::string& cert_file, const std::string& key_file) {
    for (auto& i : m_socks) {
        auto ssl_socket = std::dynamic_pointer_cast<SSLSocket>(i);
        if (ssl_socket) {
            if (!ssl_socket->loadCertificates(cert_file, key_file)) {
                return false;
            }
        }
    }
    return true;
}

std::string TcpServer::toString(const std::string& prefix) {
    std::stringstream ss;
    ss << prefix << "[type=" << m_type << " name=" << m_name << " ssl=" << m_ssl
       << " worker=" << (m_worker ? m_worker->getName() : "")
       << " accept=" << (m_acceptWorker ? m_acceptWorker->getName() : "")
       << " recv_timeout=" << m_recvTimeout << "]" << std::endl;
    std::string pfx = prefix.empty() ? "    " : prefix;
    for (auto& i : m_socks) {
        ss << pfx << pfx << *i << std::endl;
    }
    return ss.str();
}

std::vector<Socket::ptr> TcpServer::getSocks() const {
    return m_socks;
}
}  // namespace CIM