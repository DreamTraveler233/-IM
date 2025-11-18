#include "http/http.hpp"

namespace CIM::http {
/**
     * @brief 将字符串表示的HTTP方法转换为HttpMethod枚举值
     * @param[in] m HTTP方法字符串
     * @return 对应的HttpMethod枚举值，如果未找到匹配项则返回INVALID_METHOD
     *
     * @details 使用X-Macro技术遍历HTTP_METHOD_MAP中定义的所有HTTP方法，
     *          通过逐个比较输入字符串与预定义的方法字符串来确定对应的枚举值
     */
HttpMethod StringToHttpMethod(const std::string& m) {
#define XX(num, name, string)              \
    if (strcmp(#string, m.c_str()) == 0) { \
        return HttpMethod::name;           \
    }
    HTTP_METHOD_MAP(XX);
#undef XX
    return HttpMethod::INVALID_METHOD;
}

/**
     * @brief 将字符指针表示的HTTP方法转换为HttpMethod枚举值
     * @param[in] m 字符指针，指向HTTP方法字符串
     * @return 对应的HttpMethod枚举值，如果未找到匹配项则返回INVALID_METHOD
     */
HttpMethod CharsToHttpMethod(const char* m) {
#define XX(num, name, string)                        \
    if (strncmp(#string, m, strlen(#string)) == 0) { \
        return HttpMethod::name;                     \
    }
    HTTP_METHOD_MAP(XX);
#undef XX
    return HttpMethod::INVALID_METHOD;
}

static const char* s_method_string[] = {
#define XX(num, name, string) #string,
    HTTP_METHOD_MAP(XX)
#undef XX
};

/**
     * @brief 将HttpMethod枚举值转换为对应的字符串表示
     * @param[in] m HttpMethod枚举值
     * @return 对应的HTTP方法字符串，如果枚举值无效则返回"<unknown>"
     *
     * @details 通过将枚举值转换为数组索引，从预定义的字符串数组中获取对应的HTTP方法名。
     *          如果索引超出数组范围，则返回"<unknown>"表示未知方法。
     */
const char* HttpMethodToString(const HttpMethod& m) {
    uint32_t idx = (uint32_t)m;
    if (idx >= (sizeof(s_method_string) / sizeof(s_method_string[0]))) {
        return "<unknown>";
    }
    return s_method_string[idx];
}

/**
     * @brief 将HttpStatus枚举转换为对应的描述字符串
     * @param s HttpStatus枚举值
     * @return 对应的HTTP状态描述字符串，如果未找到则返回"<unknown>"
     */
const char* HttpStatusToString(const HttpStatus& s) {
    switch (s) {
#define XX(code, name, msg) \
    case HttpStatus::name:  \
        return #msg;
        HTTP_STATUS_MAP(XX);
#undef XX
        default:
            return "<unknown>";
    }
}

std::ostream& operator<<(std::ostream& os, const HttpRequest& req) {
    return req.dump(os);
}

std::ostream& operator<<(std::ostream& os, const HttpResponse& rsp) {
    return rsp.dump(os);
}

bool CaseInsensitiveLess::operator()(const std::string& lhs, const std::string& rhs) const {
    return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
}

HttpRequest::HttpRequest(uint8_t version, bool close)
    : m_method(HttpMethod::GET),
      m_path("/"),
      m_version(version),
      m_close(close),
      m_websocket(false),
      m_parserParamFlag(0) {}

std::shared_ptr<HttpResponse> HttpRequest::createResponse() {
    HttpResponse::ptr rsp(new HttpResponse(getVersion(), isClose()));
    return rsp;
}

HttpMethod HttpRequest::getMethod() const {
    return m_method;
}
uint8_t HttpRequest::getVersion() const {
    return m_version;
}
const std::string& HttpRequest::getPath() const {
    return m_path;
}
const std::string& HttpRequest::getQuery() const {
    return m_query;
}
const std::string& HttpRequest::getBody() const {
    return m_body;
}
const HttpRequest::MapType& HttpRequest::getHeaders() const {
    return m_headers;
}
const HttpRequest::MapType& HttpRequest::getParams() const {
    return m_params;
}
const HttpRequest::MapType& HttpRequest::getCookies() const {
    return m_cookies;
}
void HttpRequest::setMethod(HttpMethod v) {
    m_method = v;
}
void HttpRequest::setVersion(uint8_t v) {
    m_version = v;
}
void HttpRequest::setPath(const std::string& v) {
    m_path = v;
}
void HttpRequest::setQuery(const std::string& v) {
    m_query = v;
}
void HttpRequest::setFragment(const std::string& v) {
    m_fragment = v;
}
void HttpRequest::setBody(const std::string& v) {
    m_body = v;
}
bool HttpRequest::isClose() const {
    return m_close;
}
void HttpRequest::setClose(bool v) {
    m_close = v;
}
bool HttpRequest::isWebsocket() const {
    return m_websocket;
}
void HttpRequest::setWebsocket(bool v) {
    m_websocket = v;
}
void HttpRequest::setHeaders(const MapType& v) {
    m_headers = v;
}
void HttpRequest::setParams(const MapType& v) {
    m_params = v;
}
void HttpRequest::setCookies(const MapType& v) {
    m_cookies = v;
}
std::string HttpRequest::getHeader(const std::string& key, const std::string& def) const {
    auto it = m_headers.find(key);
    return it == m_headers.end() ? def : it->second;
}

std::string HttpRequest::getParam(const std::string& key, const std::string& def) const {
    auto it = m_params.find(key);
    return it == m_params.end() ? def : it->second;
}

std::string HttpRequest::getCookie(const std::string& key, const std::string& def) const {
    auto it = m_cookies.find(key);
    return it == m_cookies.end() ? def : it->second;
}

void HttpRequest::setHeader(const std::string& key, const std::string& val) {
    m_headers[key] = val;
}

void HttpRequest::setParam(const std::string& key, const std::string& val) {
    m_params[key] = val;
}

void HttpRequest::setCookie(const std::string& key, const std::string& val) {
    m_cookies[key] = val;
}

void HttpRequest::delHeader(const std::string& key) {
    auto it = m_headers.find(key);
    if (it != m_headers.end()) {
        m_headers.erase(it);
    }
}

void HttpRequest::delParam(const std::string& key) {
    auto it = m_params.find(key);
    if (it != m_params.end()) {
        m_params.erase(it);
    }
}

void HttpRequest::delCookie(const std::string& key) {
    auto it = m_cookies.find(key);
    if (it != m_cookies.end()) {
        m_cookies.erase(it);
    }
}

bool HttpRequest::hasHeader(const std::string& key, std::string* val) {
    auto it = m_headers.find(key);
    if (it == m_headers.end()) {
        return false;
    }
    if (val) {
        *val = it->second;
    }
    return true;
}

bool HttpRequest::hasParam(const std::string& key, std::string* val) {
    auto it = m_params.find(key);
    if (it == m_params.end()) {
        return false;
    }
    if (val) {
        *val = it->second;
    }
    return true;
}

bool HttpRequest::hasCookie(const std::string& key, std::string* val) {
    auto it = m_cookies.find(key);
    if (it == m_cookies.end()) {
        return false;
    }
    if (val) {
        *val = it->second;
    }
    return true;
}

/**
     * @brief 将HTTP请求序列化输出到流中
     * @param[in, out] os 输出流
     * @return 输出流
     *
     * 该函数将HTTP请求对象按照HTTP协议格式序列化输出到指定的输出流中，
     * 包括请求行、请求头和请求体等部分。
     *
     * 请求报文示例：
     *      GET /index.html?param=value#section1 HTTP/1.1
     *      connection: keep-alive
     *      Host: www.example.com
     *      User-Agent: Mozilla/5.0
     *      Accept: text/html,application/xhtml+xml
     *      content-length: 0
     *
     *      请求体
     */
std::ostream& HttpRequest::dump(std::ostream& os) const {
    // 输出请求行: 方法 URI HTTP版本
    os << HttpMethodToString(m_method) << " " << m_path << (m_query.empty() ? "" : "?") << m_query
       << (m_fragment.empty() ? "" : "#") << m_fragment << " HTTP/"
       << ((uint32_t)(m_version >> 4))           // 主版本号
       << "." << ((uint32_t)(m_version & 0x0F))  // 次版本号
       << "\r\n";

    // 输出连接状态信息
    if (!m_websocket) {
        os << "connection: " << (m_close ? "close" : "keep-alive") << "\r\n";
    }

    // 输出请求头信息
    for (auto& i : m_headers) {
        if (!m_websocket && strcasecmp(i.first.c_str(), "connection") == 0) {
            continue;
        }
        os << i.first << ": " << i.second << "\r\n";
    }

    // 输出请求体及长度
    if (!m_body.empty()) {
        os << "content-length: " << m_body.size() << "\r\n\r\n" << m_body;
    } else {
        os << "\r\n";
    }
    return os;
}

std::string HttpRequest::toString() const {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

void HttpRequest::init() {}

void HttpRequest::initParam() {}

void HttpRequest::initQueryParam() {}

void HttpRequest::initBodyParam() {}

void HttpRequest::initCookies() {}

HttpResponse::HttpResponse(uint8_t version, bool close)
    : m_status(HttpStatus::OK), m_version(version), m_close(close), m_websocket(false) {}

HttpStatus HttpResponse::getStatus() const {
    return m_status;
}
uint8_t HttpResponse::getVersion() const {
    return m_version;
}
const std::string& HttpResponse::getBody() const {
    return m_body;
}
const std::string& HttpResponse::getReason() const {
    return m_reason;
}
const HttpResponse::MapType& HttpResponse::getHeaders() const {
    return m_headers;
}
void HttpResponse::setStatus(HttpStatus v) {
    m_status = v;
}
void HttpResponse::setVersion(uint8_t v) {
    m_version = v;
}
void HttpResponse::setBody(const std::string& v) {
    m_body = v;
}
void HttpResponse::setReason(const std::string& v) {
    m_reason = v;
}
void HttpResponse::setHeaders(const MapType& v) {
    m_headers = v;
}
bool HttpResponse::isClose() const {
    return m_close;
}
void HttpResponse::setClose(bool v) {
    m_close = v;
}
bool HttpResponse::isWebsocket() const {
    return m_websocket;
}
void HttpResponse::setWebsocket(bool v) {
    m_websocket = v;
}
std::string HttpResponse::getHeader(const std::string& key, const std::string& def) const {
    auto it = m_headers.find(key);
    return it == m_headers.end() ? def : it->second;
}

void HttpResponse::setHeader(const std::string& key, const std::string& val) {
    m_headers[key] = val;
}

void HttpResponse::delHeader(const std::string& key) {
    auto it = m_headers.find(key);
    if (it != m_headers.end()) {
        m_headers.erase(it);
    }
}

/**
     * @brief 将HTTP响应序列化输出到流
     *
     * 该函数按照HTTP响应报文格式将HttpResponse对象的内容输出到指定的流中。
     * 输出格式遵循HTTP/1.1 RFC规范，包括状态行、响应头、空行和响应体。
     *
     * HTTP响应报文示例：
     *      HTTP/1.1 200 OK
     *      Content-Type: text/html
     *      Content-Length: 13
     *      Connection: keep-alive
     *
     *      hello world
     *
     * @param[in, out] os 输出流
     * @return 输出流
     */
std::ostream& HttpResponse::dump(std::ostream& os) const {
    // 输出状态行: HTTP版本 状态码 原因短语
    os << "HTTP/" << ((uint32_t)(m_version >> 4)) << "." << ((uint32_t)(m_version & 0x0F)) << " "
       << (uint32_t)m_status << " " << (m_reason.empty() ? HttpStatusToString(m_status) : m_reason)
       << "\r\n";

    // 输出响应头，但WebSocket连接时跳过connection头
    for (auto& i : m_headers) {
        if (!m_websocket && strcasecmp(i.first.c_str(), "connection") == 0) {
            continue;
        }
        os << i.first << ": " << i.second << "\r\n";
    }

    // 输出Cookie信息
    for (auto& i : m_cookies) {
        os << "Set-Cookie: " << i << "\r\n";
    }

    // WebSocket连接不需要设置Connection和Content-Length头
    if (!m_websocket) {
        // 显式设置连接状态
        os << "connection: " << (m_close ? "close" : "keep-alive") << "\r\n";
    }

    // 如果有响应体，则输出Content-Length头和响应体，否则只输出结尾CRLF
    if (!m_body.empty()) {
        os << "content-length: " << m_body.size() << "\r\n\r\n" << m_body;
    } else {
        os << "\r\n";
    }
    return os;
}

std::string HttpResponse::toString() const {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

void HttpResponse::setRedirect(const std::string& uri) {}

void HttpResponse::setCookie(const std::string& key, const std::string& val, time_t expired,
                             const std::string& path, const std::string& domain, bool secure) {}
}  // namespace CIM::http