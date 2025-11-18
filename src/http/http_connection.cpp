#include "http/http_connection.hpp"

#include "http/http_parser.hpp"
#include "base/macro.hpp"
#include "util/time_util.hpp"
#include "streams/zlib_stream.hpp"

namespace CIM::http {
static CIM::Logger::ptr g_logger = CIM_LOG_NAME("system");

HttpResult::HttpResult(int _result, HttpResponse::ptr _response, const std::string& _error)
    : result(_result), response(_response), error(_error) {}

std::string HttpResult::toString() const {
    std::stringstream ss;
    ss << "[HttpResult result=" << result << " error=" << error
       << " response=" << (response ? response->toString() : "nullptr") << "]";
    return ss.str();
}

HttpConnection::HttpConnection(Socket::ptr sock, bool owner) : SocketStream(sock, owner) {
    m_createTime = TimeUtil::NowToMS();
}

HttpConnection::~HttpConnection() {
    CIM_LOG_DEBUG(g_logger) << "HttpConnection::~HttpConnection";
}

HttpResponse::ptr HttpConnection::recvResponse() {
    // 创建HTTP响应解析器
    HttpResponseParser::ptr parser(new HttpResponseParser);
    // 获取HTTP请求缓冲区大小
    uint64_t buff_size = HttpRequestParser::GetHttpRequestBufferSize();
    // uint64_t buff_size = 100;
    // 分配缓冲区内存
    std::shared_ptr<char> buffer(new char[buff_size + 1], [](char* ptr) { delete[] ptr; });
    char* data = buffer.get();
    int offset = 0;

    // 循环读取数据直到解析完成
    do {
        // 从socket读取数据
        int len = read(data + offset, buff_size - offset);
        if (len <= 0) {
            // 读取失败，关闭连接并返回空指针
            close();
            return nullptr;
        }
        len += offset;
        data[len] = '\0';
        // 执行解析操作
        size_t nparse = parser->execute(data, len, false);
        if (parser->hasError()) {
            // 解析出错，关闭连接并返回空指针
            close();
            return nullptr;
        }
        // 计算未解析的数据偏移量
        offset = len - nparse;
        if (offset == (int)buff_size) {
            // 缓冲区满但未完成解析，关闭连接并返回空指针
            close();
            return nullptr;
        }
        if (parser->isFinished()) {
            // 解析完成，跳出循环
            break;
        }
    } while (true);

    // 获取底层的HTTP客户端解析器
    auto& client_parser = parser->getParser();
    std::string body;

    // 处理分块传输编码(chunked)的响应
    if (client_parser.chunked) {
        int len = offset;
        // 循环处理所有分块
        do {
            bool begin = true;
            // 处理单个分块
            do {
                // 如果不是开始或者没有数据需要读取
                if (!begin || len == 0) {
                    // 继续从socket读取数据
                    int rt = read(data + len, buff_size - len);
                    if (rt <= 0) {
                        // 读取失败，关闭连接并返回空指针
                        close();
                        return nullptr;
                    }
                    len += rt;
                }
                data[len] = '\0';
                // 执行解析操作(针对分块数据)
                size_t nparse = parser->execute(data, len, true);
                if (parser->hasError()) {
                    // 解析出错，关闭连接并返回空指针
                    close();
                    return nullptr;
                }
                // 更新未解析的数据长度
                len -= nparse;
                if (len == (int)buff_size) {
                    // 缓冲区满但未完成解析，关闭连接并返回空指针
                    close();
                    return nullptr;
                }
                begin = false;
            } while (!parser->isFinished());

            CIM_LOG_DEBUG(g_logger) << "content_len=" << client_parser.content_len;
            // 处理当前分块数据
            if (client_parser.content_len + 2 <= len) {
                // 当前缓冲区中有完整的分块数据
                body.append(data, client_parser.content_len);
                // 移动剩余数据到缓冲区开始位置
                memmove(data, data + client_parser.content_len + 2,
                        len - client_parser.content_len - 2);
                len -= client_parser.content_len + 2;
            } else {
                // 缓冲区中数据不完整，需要继续读取
                body.append(data, len);
                // 计算还需要读取的数据量(包括CRLF)
                int left = client_parser.content_len - len + 2;
                while (left > 0) {
                    // 读取剩余的分块数据
                    int rt = read(data, left > (int)buff_size ? (int)buff_size : left);
                    if (rt <= 0) {
                        // 读取失败，关闭连接并返回空指针
                        close();
                        return nullptr;
                    }
                    body.append(data, rt);
                    left -= rt;
                }
                // 移除末尾的CRLF
                body.resize(body.size() - 2);
                len = 0;
            }
        } while (!client_parser.chunks_done);  // 继续处理直到所有分块完成
    } else {
        // 处理普通(非分块)响应
        int64_t length = parser->getContentLength();
        if (length > 0) {
            // 预分配body空间
            body.resize(length);

            int len = 0;
            // 将缓冲区中已有的数据复制到body中
            if (length >= offset) {
                memcpy(&body[0], data, offset);
                len = offset;
            } else {
                memcpy(&body[0], data, length);
                len = length;
            }
            length -= offset;
            // 如果还有数据需要读取
            if (length > 0) {
                // 读取固定长度的数据
                if (readFixSize(&body[len], length) <= 0) {
                    // 读取失败，关闭连接并返回空指针
                    close();
                    return nullptr;
                }
            }
        }
    }

    // 如果响应体不为空，处理可能的内容编码
    if (!body.empty()) {
        // 获取内容编码类型
        auto content_encoding = parser->getData()->getHeader("content-encoding");
        CIM_LOG_DEBUG(g_logger) << "content_encoding: " << content_encoding
                                << " size=" << body.size();
        // 处理gzip编码
        if (strcasecmp(content_encoding.c_str(), "gzip") == 0) {
            // 创建gzip解压缩流
            auto zs = ZlibStream::CreateGzip(false);
            zs->write(body.c_str(), body.size());
            zs->flush();
            // 用解压后的数据替换原始body
            zs->getResult().swap(body);
        }
        // 处理deflate编码
        else if (strcasecmp(content_encoding.c_str(), "deflate") == 0) {
            // 创建deflate解压缩流
            auto zs = ZlibStream::CreateDeflate(false);
            zs->write(body.c_str(), body.size());
            zs->flush();
            // 用解压后的数据替换原始body
            zs->getResult().swap(body);
        }
        // 设置解析后的响应体
        parser->getData()->setBody(body);
    }
    // 返回解析后的HTTP响应数据
    return parser->getData();
}

int HttpConnection::sendRequest(HttpRequest::ptr rsp) {
    std::stringstream ss;
    ss << *rsp;
    std::string data = ss.str();
    // std::cout << ss.str() << std::endl;
    return writeFixSize(data.c_str(), data.size());
}

HttpResult::ptr HttpConnection::DoGet(const std::string& url, uint64_t timeout_ms,
                                      const std::map<std::string, std::string>& headers,
                                      const std::string& body) {
    Uri::ptr uri = Uri::Create(url);
    if (!uri) {
        return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_URL, nullptr,
                                            "invalid url: " + url);
    }
    return DoGet(uri, timeout_ms, headers, body);
}

HttpResult::ptr HttpConnection::DoGet(Uri::ptr uri, uint64_t timeout_ms,
                                      const std::map<std::string, std::string>& headers,
                                      const std::string& body) {
    return DoRequest(HttpMethod::GET, uri, timeout_ms, headers, body);
}

HttpResult::ptr HttpConnection::DoPost(const std::string& url, uint64_t timeout_ms,
                                       const std::map<std::string, std::string>& headers,
                                       const std::string& body) {
    Uri::ptr uri = Uri::Create(url);
    if (!uri) {
        return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_URL, nullptr,
                                            "invalid url: " + url);
    }
    return DoPost(uri, timeout_ms, headers, body);
}

HttpResult::ptr HttpConnection::DoPost(Uri::ptr uri, uint64_t timeout_ms,
                                       const std::map<std::string, std::string>& headers,
                                       const std::string& body) {
    return DoRequest(HttpMethod::POST, uri, timeout_ms, headers, body);
}

HttpResult::ptr HttpConnection::DoRequest(HttpMethod method, const std::string& url,
                                          uint64_t timeout_ms,
                                          const std::map<std::string, std::string>& headers,
                                          const std::string& body) {
    Uri::ptr uri = Uri::Create(url);
    if (!uri) {
        return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_URL, nullptr,
                                            "invalid url: " + url);
    }
    return DoRequest(method, uri, timeout_ms, headers, body);
}

HttpResult::ptr HttpConnection::DoRequest(HttpMethod method, Uri::ptr uri, uint64_t timeout_ms,
                                          const std::map<std::string, std::string>& headers,
                                          const std::string& body) {
    HttpRequest::ptr req = std::make_shared<HttpRequest>();
    req->setPath(uri->getPath());
    req->setQuery(uri->getQuery());
    req->setFragment(uri->getFragment());
    req->setMethod(method);
    bool has_host = false;
    for (auto& i : headers) {
        // 支持长连接
        if (strcasecmp(i.first.c_str(), "connection") == 0) {
            if (strcasecmp(i.second.c_str(), "keep-alive") == 0) {
                req->setClose(false);
            }
            continue;
        }

        // headers 中有主机号
        if (!has_host && strcasecmp(i.first.c_str(), "host") == 0) {
            has_host = !i.second.empty();
        }

        req->setHeader(i.first, i.second);
    }
    // 如果 headers 没有主机号，使用 uri 中的主机号
    if (!has_host) {
        req->setHeader("Host", uri->getHost());
    }
    req->setBody(body);
    return DoRequest(req, uri, timeout_ms);
}

HttpResult::ptr HttpConnection::DoRequest(HttpRequest::ptr req, Uri::ptr uri, uint64_t timeout_ms) {
    // 判断是否为HTTPS协议
    bool is_ssl = (uri->getScheme() == "https");

    // 从URI创建网络地址
    Address::ptr addr = uri->createAddress();
    if (!addr) {
        return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_HOST, nullptr,
                                            "invalid host: " + uri->getHost());
    }

    // 根据是否SSL创建相应类型的Socket
    Socket::ptr sock = is_ssl ? SSLSocket::CreateTCP(addr) : Socket::CreateTCP(addr);
    if (!sock) {
        return std::make_shared<HttpResult>((int)HttpResult::Error::CREATE_SOCKET_ERROR, nullptr,
                                            "create socket fail: " + addr->toString() +
                                                " errno=" + std::to_string(errno) +
                                                " errstr=" + std::string(strerror(errno)));
    }

    // 连接目标地址
    if (!sock->connect(addr)) {
        return std::make_shared<HttpResult>((int)HttpResult::Error::CONNECT_FAIL, nullptr,
                                            "connect fail: " + addr->toString());
    }

    // 设置接收超时时间
    sock->setRecvTimeout(timeout_ms);

    // 创建HTTP连接
    HttpConnection::ptr conn = std::make_shared<HttpConnection>(sock);

    // 发送HTTP请求
    int rt = conn->sendRequest(req);
    if (rt == 0) {
        return std::make_shared<HttpResult>((int)HttpResult::Error::SEND_CLOSE_BY_PEER, nullptr,
                                            "send request closed by peer: " + addr->toString());
    }
    if (rt < 0) {
        return std::make_shared<HttpResult>(
            (int)HttpResult::Error::SEND_SOCKET_ERROR, nullptr,
            "send request socket error errno=" + std::to_string(errno) +
                " errstr=" + std::string(strerror(errno)));
    }

    // 接收HTTP响应
    auto rsp = conn->recvResponse();
    if (!rsp) {
        return std::make_shared<HttpResult>((int)HttpResult::Error::TIMEOUT, nullptr,
                                            "recv response timeout: " + addr->toString() +
                                                " timeout_ms:" + std::to_string(timeout_ms));
    }

    // 返回成功结果
    return std::make_shared<HttpResult>((int)HttpResult::Error::OK, rsp, "ok");
}

HttpConnectionPool::ptr HttpConnectionPool::Create(const std::string& uri, const std::string& vhost,
                                                   uint32_t max_size, uint32_t max_alive_time,
                                                   uint32_t max_request) {
    Uri::ptr turi = Uri::Create(uri);
    if (!turi) {
        CIM_LOG_ERROR(g_logger) << "invalid uri=" << uri;
    }
    return std::make_shared<HttpConnectionPool>(turi->getHost(), vhost, turi->getPort(),
                                                turi->getScheme() == "https", max_size,
                                                max_alive_time, max_request);
}

HttpConnectionPool::HttpConnectionPool(const std::string& host, const std::string& vhost,
                                       uint32_t port, bool is_https, uint32_t max_size,
                                       uint32_t max_alive_time, uint32_t max_request)
    : m_host(host),
      m_vhost(vhost),
      m_port(port ? port : (is_https ? 443 : 80)),
      m_maxSize(max_size),
      m_maxAliveTime(max_alive_time),
      m_maxRequest(max_request),
      m_isHttps(is_https) {}

/**
     * @brief 从连接池获取一个HTTP连接
     * @return 返回一个HttpConnection智能指针，如果获取失败则返回nullptr
     *
     * 该函数首先尝试从连接池中获取一个可用的连接，如果连接池中没有可用连接
     * 或连接已失效，则创建一个新的连接。
     */
HttpConnection::ptr HttpConnectionPool::getConnection() {
    uint64_t now_ms = TimeUtil::NowToMS();       // 获取当前时间，用于判断当前连接是否超时
    std::vector<HttpConnection*> invalid_conns;  // 用于储存无效连接
    HttpConnection* ptr = nullptr;               // 用于存放取出的 connection
    MutexType::Lock lock(m_mutex);

    // 遍历连接池，查找可用连接
    while (!m_conns.empty()) {
        auto conn = *m_conns.begin();
        m_conns.pop_front();
        // 检查连接是否有效
        if (!conn->isConnected()) {
            invalid_conns.push_back(conn);
            continue;
        }
        // 检查连接是否超时
        if ((conn->m_createTime + m_maxAliveTime) < now_ms) {
            invalid_conns.push_back(conn);
            continue;
        }
        ptr = conn;
        break;
    }

    lock.unlock();

    // 删除无效连接
    for (auto i : invalid_conns) {
        delete i;
    }
    m_total -= invalid_conns.size();

    // 如果没有找到可用连接，则创建新连接
    if (!ptr) {
        IPAddress::ptr addr = Address::LookupAnyIpAddress(m_host);
        if (!addr) {
            CIM_LOG_ERROR(g_logger) << "get addr fail: " << m_host;
            return nullptr;
        }
        addr->setPort(m_port);
        // 根据是否使用HTTPS创建相应类型的Socket
        Socket::ptr sock = m_isHttps ? SSLSocket::CreateTCP(addr) : Socket::CreateTCP(addr);
        if (!sock) {
            CIM_LOG_ERROR(g_logger) << "create sock fail: " << *addr;
            return nullptr;
        }
        if (!sock->connect(addr)) {
            CIM_LOG_ERROR(g_logger) << "sock connect fail: " << *addr;
            return nullptr;
        }

        ptr = new HttpConnection(sock);
        ++m_total;
    }
    // 使用自定义删除器返回智能指针，确保连接能正确返回连接池
    return HttpConnection::ptr(
        ptr, std::bind(&HttpConnectionPool::ReleasePtr, std::placeholders::_1, this));
}

/**
     * @brief 释放HTTP连接指针回连接池
     * @param ptr 需要释放的HTTP连接指针
     * @param pool 目标连接池指针
     *
     * 该函数负责管理连接的生命周期，根据连接状态、存活时间和请求数决定是销毁连接还是放回连接池
     */
void HttpConnectionPool::ReleasePtr(HttpConnection* ptr, HttpConnectionPool* pool) {
    ++ptr->m_request;
    // 检查连接是否应该被销毁：连接已断开、超过最大存活时间或达到最大请求数
    if (!ptr->isConnected() || ((ptr->m_createTime + pool->m_maxAliveTime) < TimeUtil::NowToMS()) ||
        (ptr->m_request >= pool->m_maxRequest)) {
        delete ptr;
        --pool->m_total;
        return;
    }
    MutexType::Lock lock(pool->m_mutex);
    pool->m_conns.push_back(ptr);
}

HttpResult::ptr HttpConnectionPool::doGet(const std::string& url, uint64_t timeout_ms,
                                          const std::map<std::string, std::string>& headers,
                                          const std::string& body) {
    return doRequest(HttpMethod::GET, url, timeout_ms, headers, body);
}

HttpResult::ptr HttpConnectionPool::doGet(Uri::ptr uri, uint64_t timeout_ms,
                                          const std::map<std::string, std::string>& headers,
                                          const std::string& body) {
    std::stringstream ss;
    ss << uri->getPath() << (uri->getQuery().empty() ? "" : "?") << uri->getQuery()
       << (uri->getFragment().empty() ? "" : "#") << uri->getFragment();
    return doGet(ss.str(), timeout_ms, headers, body);
}

HttpResult::ptr HttpConnectionPool::doPost(const std::string& url, uint64_t timeout_ms,
                                           const std::map<std::string, std::string>& headers,
                                           const std::string& body) {
    return doRequest(HttpMethod::POST, url, timeout_ms, headers, body);
}

HttpResult::ptr HttpConnectionPool::doPost(Uri::ptr uri, uint64_t timeout_ms,
                                           const std::map<std::string, std::string>& headers,
                                           const std::string& body) {
    std::stringstream ss;
    ss << uri->getPath() << (uri->getQuery().empty() ? "" : "?") << uri->getQuery()
       << (uri->getFragment().empty() ? "" : "#") << uri->getFragment();
    return doPost(ss.str(), timeout_ms, headers, body);
}

/**
     * @brief 发送HTTP请求
     * @param[in] method HTTP请求方法
     * @param[in] url 请求的URL路径
     * @param[in] timeout_ms 超时时间(毫秒)
     * @param[in] headers HTTP请求头部参数
     * @param[in] body 请求消息体
     * @return 返回HTTP结果
     *
     * 该函数构造一个HTTP请求对象，并根据传入的参数设置相应字段，
     * 包括处理连接保持和主机头等细节，然后发送请求。
     */
HttpResult::ptr HttpConnectionPool::doRequest(HttpMethod method, const std::string& url,
                                              uint64_t timeout_ms,
                                              const std::map<std::string, std::string>& headers,
                                              const std::string& body) {
    HttpRequest::ptr req = std::make_shared<HttpRequest>();
    req->setPath(url);
    req->setMethod(method);
    req->setClose(false);  // 支持长连接
    bool has_host = false;

    // 处理请求头信息
    for (auto& i : headers) {
        // 处理连接相关头部，设置是否保持连接
        if (strcasecmp(i.first.c_str(), "connection") == 0) {
            if (strcasecmp(i.second.c_str(), "keep-alive") == 0) {
                req->setClose(false);
            }
            continue;
        }

        // 检查是否已提供host头部
        if (!has_host && strcasecmp(i.first.c_str(), "host") == 0) {
            has_host = !i.second.empty();
        }

        req->setHeader(i.first, i.second);
    }

    // 如果未提供host头部，则使用连接池配置的主机信息
    if (!has_host) {
        if (m_vhost.empty()) {
            req->setHeader("Host", m_host);
        } else {
            req->setHeader("Host", m_vhost);
        }
    }
    req->setBody(body);
    return doRequest(req, timeout_ms);
}

HttpResult::ptr HttpConnectionPool::doRequest(HttpMethod method, Uri::ptr uri, uint64_t timeout_ms,
                                              const std::map<std::string, std::string>& headers,
                                              const std::string& body) {
    std::stringstream ss;
    ss << uri->getPath() << (uri->getQuery().empty() ? "" : "?") << uri->getQuery()
       << (uri->getFragment().empty() ? "" : "#") << uri->getFragment();
    return doRequest(method, ss.str(), timeout_ms, headers, body);
}

HttpResult::ptr HttpConnectionPool::doRequest(HttpRequest::ptr req, uint64_t timeout_ms) {
    // 从连接池获取一个可用连接
    auto conn = getConnection();
    if (!conn) {
        return std::make_shared<HttpResult>(
            (int)HttpResult::Error::POOL_GET_CONNECTION, nullptr,
            "pool host:" + m_host + " port:" + std::to_string(m_port));
    }

    // 获取连接的Socket对象
    auto sock = conn->getSocket();
    if (!sock) {
        return std::make_shared<HttpResult>(
            (int)HttpResult::Error::POOL_INVALID_CONNECTION, nullptr,
            "pool host:" + m_host + " port:" + std::to_string(m_port));
    }

    // 设置接收超时时间并发送HTTP请求
    sock->setRecvTimeout(timeout_ms);
    int rt = conn->sendRequest(req);
    if (rt == 0) {
        return std::make_shared<HttpResult>(
            (int)HttpResult::Error::SEND_CLOSE_BY_PEER, nullptr,
            "send request closed by peer: " + sock->getRemoteAddress()->toString());
    }
    if (rt < 0) {
        return std::make_shared<HttpResult>(
            (int)HttpResult::Error::SEND_SOCKET_ERROR, nullptr,
            "send request socket error errno=" + std::to_string(errno) +
                " errstr=" + std::string(strerror(errno)));
    }

    // 接收HTTP响应
    auto rsp = conn->recvResponse();
    if (!rsp) {
        return std::make_shared<HttpResult>(
            (int)HttpResult::Error::TIMEOUT, nullptr,
            "recv response timeout: " + sock->getRemoteAddress()->toString() +
                " timeout_ms:" + std::to_string(timeout_ms));
    }

    // 返回成功结果
    return std::make_shared<HttpResult>((int)HttpResult::Error::OK, rsp, "ok");
}
}  // namespace CIM::http