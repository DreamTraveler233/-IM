#include "http/http_parser.hpp"

#include <string.h>

#include "config/config.hpp"
#include "base/macro.hpp"

namespace CIM::http {
static CIM::Logger::ptr g_logger = CIM_LOG_NAME("system");

// 加载http协议解析器的配置项
static auto g_http_request_buffer_size = CIM::Config::Lookup(
    "http.request.buffer_size", (uint64_t)(4 * 1024), "http request buffer size");
static auto g_http_request_max_body_size = CIM::Config::Lookup(
    "http.request.max_body_size", (uint64_t)(64 * 1024 * 1024), "http request max body size");
static auto g_http_response_buffer_size = CIM::Config::Lookup(
    "http.response.buffer_size", (uint64_t)(4 * 1024), "http response buffer size");
static auto g_http_response_max_body_size = CIM::Config::Lookup(
    "http.response.max_body_size", (uint64_t)(64 * 1024 * 1024), "http response max body size");

// 用于保存配置值，提升性能
static uint64_t s_http_request_buffer_size = 0;
static uint64_t s_http_request_max_body_size = 0;
static uint64_t s_http_response_buffer_size = 0;
static uint64_t s_http_response_max_body_size = 0;

uint64_t HttpRequestParser::GetHttpRequestBufferSize() {
    return s_http_request_buffer_size;
}

uint64_t HttpRequestParser::GetHttpRequestMaxBodySize() {
    return s_http_request_max_body_size;
}

uint64_t HttpResponseParser::GetHttpResponseBufferSize() {
    return s_http_response_buffer_size;
}

uint64_t HttpResponseParser::GetHttpResponseMaxBodySize() {
    return s_http_response_max_body_size;
}

namespace {
// 初始化配置
struct _RequestSizeIniter {
    _RequestSizeIniter() {
        // 获取配置
        s_http_request_buffer_size = g_http_request_buffer_size->getValue();
        s_http_request_max_body_size = g_http_request_max_body_size->getValue();
        s_http_response_buffer_size = g_http_response_buffer_size->getValue();
        s_http_response_max_body_size = g_http_response_max_body_size->getValue();

        // 设置配置修改时的回调函数
        g_http_request_buffer_size->addListener(
            [](const uint64_t& old_val, const uint64_t& new_val) {
                s_http_request_buffer_size = new_val;
            });

        g_http_request_max_body_size->addListener(
            [](const uint64_t& old_val, const uint64_t& new_val) {
                s_http_request_max_body_size = new_val;
            });

        g_http_response_buffer_size->addListener(
            [](const uint64_t& old_val, const uint64_t& new_val) {
                s_http_response_buffer_size = new_val;
            });

        g_http_response_max_body_size->addListener(
            [](const uint64_t& old_val, const uint64_t& new_val) {
                s_http_response_max_body_size = new_val;
            });
    }
};
static _RequestSizeIniter _init;
}  // namespace

/**
     * @brief HTTP请求方法解析回调函数
     * @param[in] data 指向HttpRequestParser解析器实例的指针
     * @param[in] at 指向HTTP方法字符串的指针
     * @param[in] length HTTP方法字符串的长度
     *
     * @details 该函数用于解析HTTP请求中的方法字段，将其转换为HttpMethod枚举值，
     * 并设置到对应的HttpRequest对象中。如果解析出的方法无效，则记录警告日志并将解析器错误码设为1000。
     */
void on_request_method(void* data, const char* at, size_t length) {
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    HttpMethod m = CharsToHttpMethod(at);

    if (m == HttpMethod::INVALID_METHOD) {
        CIM_LOG_WARN(g_logger) << "invalid http request method: " << std::string(at, length);
        parser->setError(1000);
        return;
    }
    parser->getData()->setMethod(m);
}

void on_request_uri(void* data, const char* at, size_t length) {}

/**
     * @brief HTTP请求片段解析回调函数
     * @param[in] data 指向HttpRequestParser解析器实例的指针
     * @param[in] at 指向URI片段字符串的指针
     * @param[in] length URI片段字符串的长度
     *
     * @details 该函数用于解析HTTP请求URI中的片段部分（#后面的部分），
     * 并将其设置到对应的HttpRequest对象中
     */
void on_request_fragment(void* data, const char* at, size_t length) {
    // CIM_LOG_INFO(g_logger) << "on_request_fragment:" << std::string(at, length);
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    parser->getData()->setFragment(std::string(at, length));
}

/**
     * @brief HTTP请求路径解析回调函数
     * @param[in] data 指向HttpRequestParser解析器实例的指针
     * @param[in] at 指向URI路径字符串的指针
     * @param[in] length URI路径字符串的长度
     *
     * @details 该函数用于解析HTTP请求URI中的路径部分（域名后的部分，问号前的部分），
     * 并将其设置到对应的HttpRequest对象中
     */
void on_request_path(void* data, const char* at, size_t length) {
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    parser->getData()->setPath(std::string(at, length));
}

/**
     * @brief HTTP请求查询参数解析回调函数
     * @param[in] data 指向HttpRequestParser解析器实例的指针
     * @param[in] at 指向查询参数字符串的指针
     * @param[in] length 查询参数字符串的长度
     *
     * @details 该函数用于解析HTTP请求URI中的查询参数部分（问号后面的部分），
     * 并将其设置到对应的HttpRequest对象中
     */
void on_request_query(void* data, const char* at, size_t length) {
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    parser->getData()->setQuery(std::string(at, length));
}

/**
     * @brief 解析HTTP请求版本
     * @param[in] data 解析器对象指针
     * @param[in] at 版本字符串起始位置
     * @param[in] length 版本字符串长度
     *
     * 该函数用于解析HTTP请求中的版本信息，支持HTTP/1.0和HTTP/1.1版本。
     * 如果版本不被支持，则记录警告日志并设置解析错误。
     */
void on_request_version(void* data, const char* at, size_t length) {
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    uint8_t v = 0;
    if (strncmp(at, "HTTP/1.1", length) == 0) {
        v = 0x11;
    } else if (strncmp(at, "HTTP/1.0", length) == 0) {
        v = 0x10;
    } else {
        CIM_LOG_WARN(g_logger) << "invalid http request version: " << std::string(at, length);
        parser->setError(1001);
        return;
    }
    parser->getData()->setVersion(v);
}

void on_request_header_done(void* data, const char* at, size_t length) {
    // HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
}

/**
     * @brief HTTP请求字段解析回调函数
     * @param[in] data 指向HttpRequestParser解析器实例的指针
     * @param[in] field HTTP头部字段名
     * @param[in] flen HTTP头部字段名长度
     * @param[in] value HTTP头部字段值
     * @param[in] vlen HTTP头部字段值长度
     *
     * @details 该函数用于解析HTTP请求中的头部字段，将字段名和字段值作为键值对
     * 设置到对应的HttpRequest对象中。如果字段名长度为0，则记录警告日志
     */
void on_request_http_field(void* data, const char* field, size_t flen, const char* value,
                           size_t vlen) {
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    if (flen == 0) {
        CIM_LOG_WARN(g_logger) << "invalid http request field length == 0";
        // parser->setError(1002);
        return;
    }
    parser->getData()->setHeader(std::string(field, flen), std::string(value, vlen));
}

HttpRequestParser::HttpRequestParser() : m_error(0) {
    m_data.reset(new CIM::http::HttpRequest);
    http_parser_init(&m_parser);
    m_parser.request_method = on_request_method;
    m_parser.request_uri = on_request_uri;
    m_parser.fragment = on_request_fragment;
    m_parser.request_path = on_request_path;
    m_parser.query_string = on_request_query;
    m_parser.http_version = on_request_version;
    m_parser.header_done = on_request_header_done;
    m_parser.http_field = on_request_http_field;
    m_parser.data = this;
}

void HttpRequestParser::setError(int v) {
    m_error = v;
}

uint64_t HttpRequestParser::getContentLength() {
    return m_data->getHeaderAs<uint64_t>("content-length", 0);
}

const http_parser& HttpRequestParser::getParser() const {
    return m_parser;
}

/**
     * @brief 解析HTTP请求协议
     * @param[in,out] data 包含HTTP协议数据的缓冲区，解析后会移除已解析的数据
     * @param[in] len 待解析数据的长度
     * @return 返回实际解析的数据长度
     */
size_t HttpRequestParser::execute(char* data, size_t len) {
    size_t offset = http_parser_execute(&m_parser, data, len, 0);
    // 数据前移，将已经处理过的数据移除，保留未处理的数据在缓冲区开头，为下一次处理做准备
    memmove(data, data + offset, (len - offset));
    return offset;
}

/**
     * @brief 检查HTTP请求解析是否完成
     * @return 返回解析完成状态
     * @retval 0 解析未完成
     * @retval 1 解析已完成
     */
int HttpRequestParser::isFinished() {
    return http_parser_finish(&m_parser);
}

/**
     * @brief 检查HTTP请求解析过程中是否发生错误
     * @return 返回错误状态，0表示无错误，非0表示有错误
     */
int HttpRequestParser::hasError() {
    return m_error || http_parser_has_error(&m_parser);
}

HttpRequest::ptr HttpRequestParser::getData() const {
    return m_data;
}

/**
     * @brief 设置HTTP响应的原因短语
     * @param[in] data 解析器对象指针
     * @param[in] at 指向原因短语字符串的指针
     * @param[in] length 原因短语字符串的长度
     */
void on_response_reason(void* data, const char* at, size_t length) {
    HttpResponseParser* parser = static_cast<HttpResponseParser*>(data);
    parser->getData()->setReason(std::string(at, length));
}

/**
     * @brief 设置HTTP响应的状态码
     * @param[in] data 解析器对象指针
     * @param[in] at 指向状态码字符串的指针
     * @param[in] length 状态码字符串的长度
     */
void on_response_status(void* data, const char* at, size_t length) {
    HttpResponseParser* parser = static_cast<HttpResponseParser*>(data);
    HttpStatus status = (HttpStatus)(atoi(at));
    parser->getData()->setStatus(status);
}

void on_response_chunk(void* data, const char* at, size_t length) {}

/**
     * @brief 解析HTTP响应版本
     * @param[in] data 解析器对象指针
     * @param[in] at 版本信息字符串起始位置
     * @param[in] length 版本信息字符串长度
     *
     * @details 根据传入的版本字符串识别HTTP协议版本，支持HTTP/1.0和HTTP/1.1。
     * 如果版本不是这两个之一，则记录警告日志并设置错误码1001。
     * 识别成功后将版本信息保存到parser的data对象中，其中0x10代表HTTP/1.0，0x11代表HTTP/1.1。
     */
void on_response_version(void* data, const char* at, size_t length) {
    HttpResponseParser* parser = static_cast<HttpResponseParser*>(data);
    uint8_t v = 0;
    if (strncmp(at, "HTTP/1.1", length) == 0) {
        v = 0x11;
    } else if (strncmp(at, "HTTP/1.0", length) == 0) {
        v = 0x10;
    } else {
        CIM_LOG_WARN(g_logger) << "invalid http response version: " << std::string(at, length);
        parser->setError(1001);
        return;
    }

    parser->getData()->setVersion(v);
}

void on_response_header_done(void* data, const char* at, size_t length) {}

void on_response_last_chunk(void* data, const char* at, size_t length) {}

/**
     * @brief 处理HTTP响应字段
     * @param[in] data 解析器对象指针
     * @param[in] field 字段名起始位置
     * @param[in] flen 字段名长度
     * @param[in] value 字段值起始位置
     * @param[in] vlen 字段值长度
     *
     * @details 解析HTTP响应中的头部字段，将字段名和字段值作为键值对存储到HttpResponse对象中。
     * 如果字段长度为0，则记录警告日志并返回。
     */
void on_response_http_field(void* data, const char* field, size_t flen, const char* value,
                            size_t vlen) {
    HttpResponseParser* parser = static_cast<HttpResponseParser*>(data);
    if (flen == 0) {
        CIM_LOG_WARN(g_logger) << "invalid http response field length == 0";
        // parser->setError(1002);
        return;
    }
    parser->getData()->setHeader(std::string(field, flen), std::string(value, vlen));
}

HttpResponseParser::HttpResponseParser() : m_error(0) {
    m_data.reset(new CIM::http::HttpResponse);
    httpclient_parser_init(&m_parser);
    m_parser.reason_phrase = on_response_reason;
    m_parser.status_code = on_response_status;
    m_parser.chunk_size = on_response_chunk;
    m_parser.http_version = on_response_version;
    m_parser.header_done = on_response_header_done;
    m_parser.last_chunk = on_response_last_chunk;
    m_parser.http_field = on_response_http_field;
    m_parser.data = this;
}

/**
     * @brief 执行HTTP响应解析
     * @param[in] data 待解析的数据
     * @param[in] len 数据长度
     * @param[in] chunck 是否为chunk模式
     * @return 解析的数据偏移量
     *
     * @details 使用httpclient_parser解析HTTP响应数据。如果chunck为true，
     * 则先初始化解析器，然后执行解析操作，并将已解析的数据从缓冲区移除，
     * 返回解析的偏移量。
     */
size_t HttpResponseParser::execute(char* data, size_t len, bool chunck) {
    if (chunck) {
        httpclient_parser_init(&m_parser);
    }
    size_t offset = httpclient_parser_execute(&m_parser, data, len, 0);
    memmove(data, data + offset, (len - offset));
    return offset;
}

/**
     * @brief 检查解析是否完成
     * @return 1表示完成，0表示未完成
     *
     * @details 调用httpclient_parser_finish检查HTTP响应解析是否已完成
     */
int HttpResponseParser::isFinished() {
    return httpclient_parser_finish(&m_parser);
}

/**
     * @brief 检查解析过程中是否有错误
     * @return 1表示有错误，0表示无错误
     *
     * @details 检查m_error成员变量或调用httpclient_parser_has_error检查是否有解析错误
     */
int HttpResponseParser::hasError() {
    return m_error || httpclient_parser_has_error(&m_parser);
}

HttpResponse::ptr HttpResponseParser::getData() const {
    return m_data;
}

void HttpResponseParser::setError(int v) {
    m_error = v;
}

uint64_t HttpResponseParser::getContentLength() {
    return m_data->getHeaderAs<uint64_t>("content-length", 0);
}

const httpclient_parser& HttpResponseParser::getParser() const {
    return m_parser;
}
}  // namespace CIM::http