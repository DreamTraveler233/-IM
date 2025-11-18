#pragma once

#include <list>

#include "http.hpp"
#include "io/lock.hpp"
#include "streams/socket_stream.hpp"
#include "uri.hpp"

namespace CIM::http {
/**
     * @brief HTTP响应结果
     */
struct HttpResult {
    /// 智能指针类型定义
    typedef std::shared_ptr<HttpResult> ptr;
    /**
         * @brief 错误码定义
         */
    enum class Error {
        OK = 0,                       /// 正常
        INVALID_URL = 1,              /// 非法URL
        INVALID_HOST = 2,             /// 无法解析HOST
        CONNECT_FAIL = 3,             /// 连接失败
        SEND_CLOSE_BY_PEER = 4,       /// 连接被对端关闭
        SEND_SOCKET_ERROR = 5,        /// 发送请求产生Socket错误
        TIMEOUT = 6,                  /// 超时
        CREATE_SOCKET_ERROR = 7,      /// 创建Socket失败
        POOL_GET_CONNECTION = 8,      /// 从连接池中取连接失败
        POOL_INVALID_CONNECTION = 9,  /// 无效的连接
    };

    /**
         * @brief 构造函数
         * @param[in] _result 错误码
         * @param[in] _response HTTP响应结构体
         * @param[in] _error 错误描述
         */
    HttpResult(int _result, HttpResponse::ptr _response, const std::string& _error);
    std::string toString() const;

    int result;                  /// 错误码
    HttpResponse::ptr response;  /// HTTP响应结构体
    std::string error;           /// 错误描述
};

class HttpConnectionPool;
/**
     * @brief HTTP客户端类
     */
class HttpConnection : public SocketStream {
    friend class HttpConnectionPool;

   public:
    /// HTTP客户端类智能指针
    typedef std::shared_ptr<HttpConnection> ptr;

    /**
         * @brief 构造函数
         * @param[in] sock Socket类
         * @param[in] owner 是否掌握所有权
         */
    HttpConnection(Socket::ptr sock, bool owner = true);

    /**
         * @brief 析构函数
         */
    ~HttpConnection();

    /**
         * @brief 发送HTTP的GET请求
         * @param[in] url 请求的url
         * @param[in] timeout_ms 超时时间(毫秒)
         * @param[in] headers HTTP请求头部参数
         * @param[in] body 请求消息体
         * @return 返回HTTP结果结构体
         */
    static HttpResult::ptr DoGet(const std::string& url, uint64_t timeout_ms,
                                 const std::map<std::string, std::string>& headers = {},
                                 const std::string& body = "");

    /**
         * @brief 发送HTTP的GET请求
         * @param[in] uri URI结构体
         * @param[in] timeout_ms 超时时间(毫秒)
         * @param[in] headers HTTP请求头部参数
         * @param[in] body 请求消息体
         * @return 返回HTTP结果结构体
         */
    static HttpResult::ptr DoGet(Uri::ptr uri, uint64_t timeout_ms,
                                 const std::map<std::string, std::string>& headers = {},
                                 const std::string& body = "");

    /**
         * @brief 发送HTTP的POST请求
         * @param[in] url 请求的url
         * @param[in] timeout_ms 超时时间(毫秒)
         * @param[in] headers HTTP请求头部参数
         * @param[in] body 请求消息体
         * @return 返回HTTP结果结构体
         */
    static HttpResult::ptr DoPost(const std::string& url, uint64_t timeout_ms,
                                  const std::map<std::string, std::string>& headers = {},
                                  const std::string& body = "");

    /**
         * @brief 发送HTTP的POST请求
         * @param[in] uri URI结构体
         * @param[in] timeout_ms 超时时间(毫秒)
         * @param[in] headers HTTP请求头部参数
         * @param[in] body 请求消息体
         * @return 返回HTTP结果结构体
         */
    static HttpResult::ptr DoPost(Uri::ptr uri, uint64_t timeout_ms,
                                  const std::map<std::string, std::string>& headers = {},
                                  const std::string& body = "");

    /**
         * @brief 发送HTTP请求
         * @param[in] method 请求类型
         * @param[in] uri 请求的url
         * @param[in] timeout_ms 超时时间(毫秒)
         * @param[in] headers HTTP请求头部参数
         * @param[in] body 请求消息体
         * @return 返回HTTP结果结构体
         */
    static HttpResult::ptr DoRequest(HttpMethod method, const std::string& url, uint64_t timeout_ms,
                                     const std::map<std::string, std::string>& headers = {},
                                     const std::string& body = "");

    /**
         * @brief 发送HTTP请求
         * @param[in] method 请求类型
         * @param[in] uri URI结构体
         * @param[in] timeout_ms 超时时间(毫秒)
         * @param[in] headers HTTP请求头部参数
         * @param[in] body 请求消息体
         * @return 返回HTTP结果结构体
         */
    static HttpResult::ptr DoRequest(HttpMethod method, Uri::ptr uri, uint64_t timeout_ms,
                                     const std::map<std::string, std::string>& headers = {},
                                     const std::string& body = "");

    /**
         * @brief 发送HTTP请求
         * @param[in] req 请求结构体
         * @param[in] uri URI结构体
         * @param[in] timeout_ms 超时时间(毫秒)
         * @return 返回HTTP结果结构体
         */
    static HttpResult::ptr DoRequest(HttpRequest::ptr req, Uri::ptr uri, uint64_t timeout_ms);

    /**
         * @brief 接收HTTP响应
         */
    HttpResponse::ptr recvResponse();

    /**
         * @brief 发送HTTP请求
         * @param[in] req HTTP请求结构
         */
    int sendRequest(HttpRequest::ptr req);

   private:
    uint64_t m_createTime = 0;  /// 连接创建时间
    uint64_t m_request = 0;     /// 请求次数
};

class HttpConnectionPool {
   public:
    typedef std::shared_ptr<HttpConnectionPool> ptr;
    typedef Mutex MutexType;

    static HttpConnectionPool::ptr Create(const std::string& uri, const std::string& vhost,
                                          uint32_t max_size, uint32_t max_alive_time,
                                          uint32_t max_request);

    HttpConnectionPool(const std::string& host, const std::string& vhost, uint32_t port,
                       bool is_https, uint32_t max_size, uint32_t max_alive_time,
                       uint32_t max_request);

    HttpConnection::ptr getConnection();

    /**
         * @brief 发送HTTP的GET请求
         * @param[in] url 请求的url
         * @param[in] timeout_ms 超时时间(毫秒)
         * @param[in] headers HTTP请求头部参数
         * @param[in] body 请求消息体
         * @return 返回HTTP结果结构体
         */
    HttpResult::ptr doGet(const std::string& url, uint64_t timeout_ms,
                          const std::map<std::string, std::string>& headers = {},
                          const std::string& body = "");

    /**
         * @brief 发送HTTP的GET请求
         * @param[in] uri URI结构体
         * @param[in] timeout_ms 超时时间(毫秒)
         * @param[in] headers HTTP请求头部参数
         * @param[in] body 请求消息体
         * @return 返回HTTP结果结构体
         */
    HttpResult::ptr doGet(Uri::ptr uri, uint64_t timeout_ms,
                          const std::map<std::string, std::string>& headers = {},
                          const std::string& body = "");

    /**
         * @brief 发送HTTP的POST请求
         * @param[in] url 请求的url
         * @param[in] timeout_ms 超时时间(毫秒)
         * @param[in] headers HTTP请求头部参数
         * @param[in] body 请求消息体
         * @return 返回HTTP结果结构体
         */
    HttpResult::ptr doPost(const std::string& url, uint64_t timeout_ms,
                           const std::map<std::string, std::string>& headers = {},
                           const std::string& body = "");

    /**
         * @brief 发送HTTP的POST请求
         * @param[in] uri URI结构体
         * @param[in] timeout_ms 超时时间(毫秒)
         * @param[in] headers HTTP请求头部参数
         * @param[in] body 请求消息体
         * @return 返回HTTP结果结构体
         */
    HttpResult::ptr doPost(Uri::ptr uri, uint64_t timeout_ms,
                           const std::map<std::string, std::string>& headers = {},
                           const std::string& body = "");

    /**
         * @brief 发送HTTP请求
         * @param[in] method 请求类型
         * @param[in] uri 请求的url
         * @param[in] timeout_ms 超时时间(毫秒)
         * @param[in] headers HTTP请求头部参数
         * @param[in] body 请求消息体
         * @return 返回HTTP结果结构体
         */
    HttpResult::ptr doRequest(HttpMethod method, const std::string& url, uint64_t timeout_ms,
                              const std::map<std::string, std::string>& headers = {},
                              const std::string& body = "");

    /**
         * @brief 发送HTTP请求
         * @param[in] method 请求类型
         * @param[in] uri URI结构体
         * @param[in] timeout_ms 超时时间(毫秒)
         * @param[in] headers HTTP请求头部参数
         * @param[in] body 请求消息体
         * @return 返回HTTP结果结构体
         */
    HttpResult::ptr doRequest(HttpMethod method, Uri::ptr uri, uint64_t timeout_ms,
                              const std::map<std::string, std::string>& headers = {},
                              const std::string& body = "");

    /**
         * @brief 发送HTTP请求
         * @param[in] req 请求结构体
         * @param[in] timeout_ms 超时时间(毫秒)
         * @return 返回HTTP结果结构体
         */
    HttpResult::ptr doRequest(HttpRequest::ptr req, uint64_t timeout_ms);

   private:
    static void ReleasePtr(HttpConnection* ptr, HttpConnectionPool* pool);

   private:
    std::string m_host;
    std::string m_vhost;
    uint32_t m_port;
    uint32_t m_maxSize;       /// 连接池中的最大连接数
    uint32_t m_maxAliveTime;  /// 最长连接时间
    uint32_t m_maxRequest;    /// 单个连接最大请求次数
    bool m_isHttps;

    MutexType m_mutex;
    std::list<HttpConnection*> m_conns;  /// 连接池
    std::atomic<int32_t> m_total = {0};  /// 当前连接池的连接数
};
}  // namespace CIM::http