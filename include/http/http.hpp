#pragma once

#include <boost/lexical_cast.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "http11_parser.hpp"
#include "httpclient_parser.hpp"

namespace CIM::http {
// X-Macro技术（也称为"交叉宏"技术）
/* Request Methods */
#define HTTP_METHOD_MAP(XX)          \
    XX(0, DELETE, DELETE)            \
    XX(1, GET, GET)                  \
    XX(2, HEAD, HEAD)                \
    XX(3, POST, POST)                \
    XX(4, PUT, PUT)                  \
    /* pathological */               \
    XX(5, CONNECT, CONNECT)          \
    XX(6, OPTIONS, OPTIONS)          \
    XX(7, TRACE, TRACE)              \
    /* WebDAV */                     \
    XX(8, COPY, COPY)                \
    XX(9, LOCK, LOCK)                \
    XX(10, MKCOL, MKCOL)             \
    XX(11, MOVE, MOVE)               \
    XX(12, PROPFIND, PROPFIND)       \
    XX(13, PROPPATCH, PROPPATCH)     \
    XX(14, SEARCH, SEARCH)           \
    XX(15, UNLOCK, UNLOCK)           \
    XX(16, BIND, BIND)               \
    XX(17, REBIND, REBIND)           \
    XX(18, UNBIND, UNBIND)           \
    XX(19, ACL, ACL)                 \
    /* subversion */                 \
    XX(20, REPORT, REPORT)           \
    XX(21, MKACTIVITY, MKACTIVITY)   \
    XX(22, CHECKOUT, CHECKOUT)       \
    XX(23, MERGE, MERGE)             \
    /* upnp */                       \
    XX(24, MSEARCH, M - SEARCH)      \
    XX(25, NOTIFY, NOTIFY)           \
    XX(26, SUBSCRIBE, SUBSCRIBE)     \
    XX(27, UNSUBSCRIBE, UNSUBSCRIBE) \
    /* RFC-5789 */                   \
    XX(28, PATCH, PATCH)             \
    XX(29, PURGE, PURGE)             \
    /* CalDAV */                     \
    XX(30, MKCALENDAR, MKCALENDAR)   \
    /* RFC-2068, section 19.6.1.2 */ \
    XX(31, LINK, LINK)               \
    XX(32, UNLINK, UNLINK)           \
    /* icecast */                    \
    XX(33, SOURCE, SOURCE)

/* Status Codes */
#define HTTP_STATUS_MAP(XX)                                                   \
    XX(100, CONTINUE, Continue)                                               \
    XX(101, SWITCHING_PROTOCOLS, Switching Protocols)                         \
    XX(102, PROCESSING, Processing)                                           \
    XX(200, OK, OK)                                                           \
    XX(201, CREATED, Created)                                                 \
    XX(202, ACCEPTED, Accepted)                                               \
    XX(203, NON_AUTHORITATIVE_INFORMATION, Non - Authoritative Information)   \
    XX(204, NO_CONTENT, No Content)                                           \
    XX(205, RESET_CONTENT, Reset Content)                                     \
    XX(206, PARTIAL_CONTENT, Partial Content)                                 \
    XX(207, MULTI_STATUS, Multi - Status)                                     \
    XX(208, ALREADY_REPORTED, Already Reported)                               \
    XX(226, IM_USED, IM Used)                                                 \
    XX(300, MULTIPLE_CHOICES, Multiple Choices)                               \
    XX(301, MOVED_PERMANENTLY, Moved Permanently)                             \
    XX(302, FOUND, Found)                                                     \
    XX(303, SEE_OTHER, See Other)                                             \
    XX(304, NOT_MODIFIED, Not Modified)                                       \
    XX(305, USE_PROXY, Use Proxy)                                             \
    XX(307, TEMPORARY_REDIRECT, Temporary Redirect)                           \
    XX(308, PERMANENT_REDIRECT, Permanent Redirect)                           \
    XX(400, BAD_REQUEST, Bad Request)                                         \
    XX(401, UNAUTHORIZED, Unauthorized)                                       \
    XX(402, PAYMENT_REQUIRED, Payment Required)                               \
    XX(403, FORBIDDEN, Forbidden)                                             \
    XX(404, NOT_FOUND, Not Found)                                             \
    XX(405, METHOD_NOT_ALLOWED, Method Not Allowed)                           \
    XX(406, NOT_ACCEPTABLE, Not Acceptable)                                   \
    XX(407, PROXY_AUTHENTICATION_REQUIRED, Proxy Authentication Required)     \
    XX(408, REQUEST_TIMEOUT, Request Timeout)                                 \
    XX(409, CONFLICT, Conflict)                                               \
    XX(410, GONE, Gone)                                                       \
    XX(411, LENGTH_REQUIRED, Length Required)                                 \
    XX(412, PRECONDITION_FAILED, Precondition Failed)                         \
    XX(413, PAYLOAD_TOO_LARGE, Payload Too Large)                             \
    XX(414, URI_TOO_LONG, URI Too Long)                                       \
    XX(415, UNSUPPORTED_MEDIA_TYPE, Unsupported Media Type)                   \
    XX(416, RANGE_NOT_SATISFIABLE, Range Not Satisfiable)                     \
    XX(417, EXPECTATION_FAILED, Expectation Failed)                           \
    XX(421, MISDIRECTED_REQUEST, Misdirected Request)                         \
    XX(422, UNPROCESSABLE_ENTITY, Unprocessable Entity)                       \
    XX(423, LOCKED, Locked)                                                   \
    XX(424, FAILED_DEPENDENCY, Failed Dependency)                             \
    XX(426, UPGRADE_REQUIRED, Upgrade Required)                               \
    XX(428, PRECONDITION_REQUIRED, Precondition Required)                     \
    XX(429, TOO_MANY_REQUESTS, Too Many Requests)                             \
    XX(431, REQUEST_HEADER_FIELDS_TOO_LARGE, Request Header Fields Too Large) \
    XX(451, UNAVAILABLE_FOR_LEGAL_REASONS, Unavailable For Legal Reasons)     \
    XX(500, INTERNAL_SERVER_ERROR, Internal Server Error)                     \
    XX(501, NOT_IMPLEMENTED, Not Implemented)                                 \
    XX(502, BAD_GATEWAY, Bad Gateway)                                         \
    XX(503, SERVICE_UNAVAILABLE, Service Unavailable)                         \
    XX(504, GATEWAY_TIMEOUT, Gateway Timeout)                                 \
    XX(505, HTTP_VERSION_NOT_SUPPORTED, HTTP Version Not Supported)           \
    XX(506, VARIANT_ALSO_NEGOTIATES, Variant Also Negotiates)                 \
    XX(507, INSUFFICIENT_STORAGE, Insufficient Storage)                       \
    XX(508, LOOP_DETECTED, Loop Detected)                                     \
    XX(510, NOT_EXTENDED, Not Extended)                                       \
    XX(511, NETWORK_AUTHENTICATION_REQUIRED, Network Authentication Required)

/**
     * @brief HTTP方法枚举
     */
enum class HttpMethod {
#define XX(num, name, string) name = num,
    HTTP_METHOD_MAP(XX)
#undef XX
        INVALID_METHOD  // 无效的HTTP方法
};

/**
     * @brief HTTP状态枚举
     */
enum class HttpStatus {
#define XX(code, name, desc) name = code,
    HTTP_STATUS_MAP(XX)
#undef XX
};

// 将字符串转换为HttpMethod枚举值
HttpMethod StringToHttpMethod(const std::string& m);
// 将字符数组转换为HttpMethod枚举值
HttpMethod CharsToHttpMethod(const char* m);
// 将HttpMethod枚举值转换为字符串
const char* HttpMethodToString(const HttpMethod& m);
// 将HttpStatus枚举值转换为描述字符串
const char* HttpStatusToString(const HttpStatus& s);

/**
     * @brief 忽略大小写比较仿函数
     */
struct CaseInsensitiveLess {
    /**
         * @brief 忽略大小写比较字符串
         */
    bool operator()(const std::string& lhs, const std::string& rhs) const;
};

/**
     * @brief 获取Map中的key值,并转成对应类型,返回是否成功
     * @param[in] m Map数据结构
     * @param[in] key 关键字
     * @param[out] val 保存转换后的值
     * @param[in] def 默认值
     * @return
     *      @retval true 转换成功, val 为对应的值
     *      @retval false 不存在或者转换失败 val = def
     */
template <class MapType, class T>
bool checkGetAs(const MapType& m, const std::string& key, T& val, const T& def = T()) {
    auto it = m.find(key);
    if (it == m.end()) {
        val = def;
        return false;
    }
    try {
        val = boost::lexical_cast<T>(it->second);
        return true;
    } catch (...) {
        val = def;
    }
    return false;
}

/**
     * @brief 获取Map中的key值,并转成对应类型
     * @param[in] m Map数据结构
     * @param[in] key 关键字
     * @param[in] def 默认值
     * @return 如果存在且转换成功返回对应的值,否则返回默认值
     */
template <class MapType, class T>
T getAs(const MapType& m, const std::string& key, const T& def = T()) {
    auto it = m.find(key);
    if (it == m.end()) {
        return def;
    }
    try {
        return boost::lexical_cast<T>(it->second);
    } catch (...) {
    }
    return def;
}

class HttpResponse;
/**
     * @brief HTTP请求结构
     */
class HttpRequest {
   public:
    /// HTTP请求的智能指针
    using ptr = std::shared_ptr<HttpRequest>;
    /// MAP结构
    using MapType = std::map<std::string, std::string, CaseInsensitiveLess>;

    /**
         * @brief 构造函数
         * @param[in] version 版本
         * @param[in] close 是否keepalive
         */
    HttpRequest(uint8_t version = 0x11, bool close = true);

    std::shared_ptr<HttpResponse> createResponse();

    /**
         * @brief 返回HTTP方法
         */
    HttpMethod getMethod() const;

    /**
         * @brief 返回HTTP版本
         */
    uint8_t getVersion() const;

    /**
         * @brief 返回HTTP请求的路径
         */
    const std::string& getPath() const;

    /**
         * @brief 返回HTTP请求的查询参数
         */
    const std::string& getQuery() const;

    /**
         * @brief 返回HTTP请求的消息体
         */
    const std::string& getBody() const;

    /**
         * @brief 返回HTTP请求的消息头MAP
         */
    const MapType& getHeaders() const;

    /**
         * @brief 返回HTTP请求的参数MAP
         */
    const MapType& getParams() const;

    /**
         * @brief 返回HTTP请求的cookie MAP
         */
    const MapType& getCookies() const;

    /**
         * @brief 设置HTTP请求的方法名
         * @param[in] v HTTP请求
         */
    void setMethod(HttpMethod v);

    /**
         * @brief 设置HTTP请求的协议版本
         * @param[in] v 协议版本0x11, 0x10
         */
    void setVersion(uint8_t v);

    /**
         * @brief 设置HTTP请求的路径
         * @param[in] v 请求路径
         */
    void setPath(const std::string& v);

    /**
         * @brief 设置HTTP请求的查询参数
         * @param[in] v 查询参数
         */
    void setQuery(const std::string& v);

    /**
         * @brief 设置HTTP请求的Fragment
         * @param[in] v fragment
         */
    void setFragment(const std::string& v);

    /**
         * @brief 设置HTTP请求的消息体
         * @param[in] v 消息体
         */
    void setBody(const std::string& v);

    /**
         * @brief 是否自动关闭
         */
    bool isClose() const;

    /**
         * @brief 设置是否自动关闭
         */
    void setClose(bool v);

    /**
         * @brief 是否websocket
         */
    bool isWebsocket() const;

    /**
         * @brief 设置是否websocket
         */
    void setWebsocket(bool v);

    /**
         * @brief 设置HTTP请求的头部MAP
         * @param[in] v map
         */
    void setHeaders(const MapType& v);

    /**
         * @brief 设置HTTP请求的参数MAP
         * @param[in] v map
         */
    void setParams(const MapType& v);

    /**
         * @brief 设置HTTP请求的Cookie MAP
         * @param[in] v map
         */
    void setCookies(const MapType& v);

    /**
         * @brief 获取HTTP请求的头部参数
         * @param[in] key 关键字
         * @param[in] def 默认值
         * @return 如果存在则返回对应值,否则返回默认值
         */
    std::string getHeader(const std::string& key, const std::string& def = "") const;

    /**
         * @brief 获取HTTP请求的请求参数
         * @param[in] key 关键字
         * @param[in] def 默认值
         * @return 如果存在则返回对应值,否则返回默认值
         */
    std::string getParam(const std::string& key, const std::string& def = "") const;

    /**
         * @brief 获取HTTP请求的Cookie参数
         * @param[in] key 关键字
         * @param[in] def 默认值
         * @return 如果存在则返回对应值,否则返回默认值
         */
    std::string getCookie(const std::string& key, const std::string& def = "") const;

    /**
         * @brief 设置HTTP请求的头部参数
         * @param[in] key 关键字
         * @param[in] val 值
         */
    void setHeader(const std::string& key, const std::string& val);

    /**
         * @brief 设置HTTP请求的请求参数
         * @param[in] key 关键字
         * @param[in] val 值
         */

    void setParam(const std::string& key, const std::string& val);
    /**
         * @brief 设置HTTP请求的Cookie参数
         * @param[in] key 关键字
         * @param[in] val 值
         */
    void setCookie(const std::string& key, const std::string& val);

    /**
         * @brief 删除HTTP请求的头部参数
         * @param[in] key 关键字
         */
    void delHeader(const std::string& key);

    /**
         * @brief 删除HTTP请求的请求参数
         * @param[in] key 关键字
         */
    void delParam(const std::string& key);

    /**
         * @brief 删除HTTP请求的Cookie参数
         * @param[in] key 关键字
         */
    void delCookie(const std::string& key);

    /**
         * @brief 判断HTTP请求的头部参数是否存在
         * @param[in] key 关键字
         * @param[out] val 如果存在,val非空则赋值
         * @return 是否存在
         */
    bool hasHeader(const std::string& key, std::string* val = nullptr);

    /**
         * @brief 判断HTTP请求的请求参数是否存在
         * @param[in] key 关键字
         * @param[out] val 如果存在,val非空则赋值
         * @return 是否存在
         */
    bool hasParam(const std::string& key, std::string* val = nullptr);

    /**
         * @brief 判断HTTP请求的Cookie参数是否存在
         * @param[in] key 关键字
         * @param[out] val 如果存在,val非空则赋值
         * @return 是否存在
         */
    bool hasCookie(const std::string& key, std::string* val = nullptr);

    /**
         * @brief 检查并获取HTTP请求的头部参数
         * @tparam T 转换类型
         * @param[in] key 关键字
         * @param[out] val 返回值
         * @param[in] def 默认值
         * @return 如果存在且转换成功返回true,否则失败val=def
         */
    template <class T>
    bool checkGetHeaderAs(const std::string& key, T& val, const T& def = T()) {
        return checkGetAs(m_headers, key, val, def);
    }

    /**
         * @brief 获取HTTP请求的头部参数
         * @tparam T 转换类型
         * @param[in] key 关键字
         * @param[in] def 默认值
         * @return 如果存在且转换成功返回对应的值,否则返回def
         */
    template <class T>
    T getHeaderAs(const std::string& key, const T& def = T()) {
        return getAs(m_headers, key, def);
    }

    /**
         * @brief 检查并获取HTTP请求的请求参数
         * @tparam T 转换类型
         * @param[in] key 关键字
         * @param[out] val 返回值
         * @param[in] def 默认值
         * @return 如果存在且转换成功返回true,否则失败val=def
         */
    template <class T>
    bool checkGetParamAs(const std::string& key, T& val, const T& def = T()) {
        initQueryParam();
        initBodyParam();
        return checkGetAs(m_params, key, val, def);
    }

    /**
         * @brief 获取HTTP请求的请求参数
         * @tparam T 转换类型
         * @param[in] key 关键字
         * @param[in] def 默认值
         * @return 如果存在且转换成功返回对应的值,否则返回def
         */
    template <class T>
    T getParamAs(const std::string& key, const T& def = T()) {
        initQueryParam();
        initBodyParam();
        return getAs(m_params, key, def);
    }

    /**
         * @brief 检查并获取HTTP请求的Cookie参数
         * @tparam T 转换类型
         * @param[in] key 关键字
         * @param[out] val 返回值
         * @param[in] def 默认值
         * @return 如果存在且转换成功返回true,否则失败val=def
         */
    template <class T>
    bool checkGetCookieAs(const std::string& key, T& val, const T& def = T()) {
        initCookies();
        return checkGetAs(m_cookies, key, val, def);
    }

    /**
         * @brief 获取HTTP请求的Cookie参数
         * @tparam T 转换类型
         * @param[in] key 关键字
         * @param[in] def 默认值
         * @return 如果存在且转换成功返回对应的值,否则返回def
         */
    template <class T>
    T getCookieAs(const std::string& key, const T& def = T()) {
        initCookies();
        return getAs(m_cookies, key, def);
    }

    /**
         * @brief 序列化输出到流中
         * @param[in, out] os 输出流
         * @return 输出流
         */
    std::ostream& dump(std::ostream& os) const;

    /**
         * @brief 转成字符串类型
         * @return 字符串
         */
    std::string toString() const;

    // 初始化HTTP请求相关参数
    void init();
    // 初始化所有参数
    void initParam();
    // 初始化查询参数
    void initQueryParam();
    // 初始化请求体参数
    void initBodyParam();
    // 初始化Cookie
    void initCookies();

   private:
    HttpMethod m_method;     /// HTTP方法
    std::string m_path;      /// 请求路径
    std::string m_query;     /// 查询参数
    uint8_t m_version;       /// HTTP版本
    MapType m_headers;       /// 请求头
    MapType m_cookies;       /// Cookie
    std::string m_body;      /// 请求体
    MapType m_params;        /// 请求参数
    std::string m_fragment;  /// Fragment

    bool m_close;               /// 是否自动关闭
    bool m_websocket;           /// 是否为websocket
    uint8_t m_parserParamFlag;  /// 参数是否已解析
};

/**
     * @brief HTTP响应结构体
     */
class HttpResponse {
   public:
    /// HTTP响应结构智能指针
    using ptr = std::shared_ptr<HttpResponse>;
    /// MapType
    using MapType = std::map<std::string, std::string, CaseInsensitiveLess>;

    /**
         * @brief 构造函数
         * @param[in] version 版本
         * @param[in] close 是否自动关闭
         */
    HttpResponse(uint8_t version = 0x11, bool close = true);

    /**
         * @brief 返回响应状态
         * @return 请求状态
         */
    HttpStatus getStatus() const;

    /**
         * @brief 返回响应版本
         * @return 版本
         */
    uint8_t getVersion() const;

    /**
         * @brief 返回响应消息体
         * @return 消息体
         */
    const std::string& getBody() const;

    /**
         * @brief 返回响应原因
         */
    const std::string& getReason() const;

    /**
         * @brief 返回响应头部MAP
         * @return MAP
         */
    const MapType& getHeaders() const;

    /**
         * @brief 设置响应状态
         * @param[in] v 响应状态
         */
    void setStatus(HttpStatus v);

    /**
         * @brief 设置响应版本
         * @param[in] v 版本
         */
    void setVersion(uint8_t v);

    /**
         * @brief 设置响应消息体
         * @param[in] v 消息体
         */
    void setBody(const std::string& v);

    /**
         * @brief 设置响应原因
         * @param[in] v 原因
         */
    void setReason(const std::string& v);

    /**
         * @brief 设置响应头部MAP
         * @param[in] v MAP
         */
    void setHeaders(const MapType& v);

    /**
         * @brief 是否自动关闭
         */
    bool isClose() const;

    /**
         * @brief 设置是否自动关闭
         */
    void setClose(bool v);

    /**
         * @brief 是否websocket
         */
    bool isWebsocket() const;

    /**
         * @brief 设置是否websocket
         */
    void setWebsocket(bool v);

    /**
         * @brief 获取响应头部参数
         * @param[in] key 关键字
         * @param[in] def 默认值
         * @return 如果存在返回对应值,否则返回def
         */
    std::string getHeader(const std::string& key, const std::string& def = "") const;

    /**
         * @brief 设置响应头部参数
         * @param[in] key 关键字
         * @param[in] val 值
         */
    void setHeader(const std::string& key, const std::string& val);

    /**
         * @brief 删除响应头部参数
         * @param[in] key 关键字
         */
    void delHeader(const std::string& key);

    /**
         * @brief 检查并获取响应头部参数
         * @tparam T 值类型
         * @param[in] key 关键字
         * @param[out] val 值
         * @param[in] def 默认值
         * @return 如果存在且转换成功返回true,否则失败val=def
         */
    template <class T>
    bool checkGetHeaderAs(const std::string& key, T& val, const T& def = T()) {
        return checkGetAs(m_headers, key, val, def);
    }

    /**
         * @brief 获取响应的头部参数
         * @tparam T 转换类型
         * @param[in] key 关键字
         * @param[in] def 默认值
         * @return 如果存在且转换成功返回对应的值,否则返回def
         */
    template <class T>
    T getHeaderAs(const std::string& key, const T& def = T()) {
        return getAs(m_headers, key, def);
    }

    /**
         * @brief 序列化输出到流
         * @param[in, out] os 输出流
         * @return 输出流
         */
    std::ostream& dump(std::ostream& os) const;

    /**
         * @brief 转成字符串
         */
    std::string toString() const;

    /**
         * @brief 设置重定向地址
         * @param[in] uri 重定向的URI
         */
    void setRedirect(const std::string& uri);

    /**
         * @brief 设置Cookie
         * @param[in] key Cookie键
         * @param[in] val Cookie值
         * @param[in] expired 过期时间
         * @param[in] path Cookie路径
         * @param[in] domain Cookie域
         * @param[in] secure 是否安全连接
         */
    void setCookie(const std::string& key, const std::string& val, time_t expired = 0,
                   const std::string& path = "", const std::string& domain = "",
                   bool secure = false);

   private:
    HttpStatus m_status;                 /// 响应状态
    uint8_t m_version;                   /// 版本
    bool m_close;                        /// 是否自动关闭
    bool m_websocket;                    /// 是否为websocket
    std::string m_body;                  /// 响应消息体
    std::string m_reason;                /// 响应原因
    MapType m_headers;                   /// 响应头部MAP
    std::vector<std::string> m_cookies;  /// Cookie列表
};

/**
     * @brief 流式输出HttpRequest
     * @param[in, out] os 输出流
     * @param[in] req HTTP请求
     * @return 输出流
     */
std::ostream& operator<<(std::ostream& os, const HttpRequest& req);

/**
     * @brief 流式输出HttpResponse
     * @param[in, out] os 输出流
     * @param[in] rsp HTTP响应
     * @return 输出流
     */
std::ostream& operator<<(std::ostream& os, const HttpResponse& rsp);
}  // namespace CIM::http