/**
 * @file http11_parser.hpp
 * @brief HTTP/1.1 请求解析器
 * 
 * 本文件定义了HTTP/1.1请求解析器的核心结构和接口，基于Ragel状态机工具生成的C代码封装。
 * 主要功能包括HTTP请求行解析、HTTP头部字段解析以及相关回调函数的定义。
 * 
 * 该解析器支持以下HTTP/1.1特性：
 * - HTTP方法解析（GET、POST等）
 * - URI解析（路径、查询参数、Fragment）
 * - HTTP版本解析
 * - HTTP头部字段解析
 * - 支持宽松模式的URI解析
 * 
 * 解析过程通过回调函数通知上层应用解析到的数据，包括请求方法、URI、HTTP版本、
 * 头部字段等信息。解析器本身不存储解析结果，而是通过回调函数传递给调用者。
 */

#pragma once

#include "http11_common.hpp"

/**
 * @brief HTTP请求解析器结构体
 * 
 * 该结构体用于存储HTTP请求解析过程中的状态和数据，
 * 是基于Ragel状态机生成的C代码包装
 */
typedef struct http_parser {
    int cs;              /// 当前状态机状态
    size_t body_start;   /// HTTP消息体开始位置
    int content_len;     /// 内容长度
    size_t nread;        /// 已读取的字节数
    size_t mark;         /// 标记位置，用于记录解析过程中的关键位置
    size_t field_start;  /// 当前HTTP头部字段开始位置
    size_t field_len;    /// 当前HTTP头部字段长度
    size_t query_start;  /// 查询字符串开始位置
    int xml_sent;        /// 是否已发送XML数据
    int json_sent;       /// 是否已发送JSON数据

    void* data;  /// 用户自定义数据指针，用于回调函数传递数据

    int uri_relaxed;            /// URI宽松模式标记
    field_cb http_field;        /// HTTP头部字段回调函数指针
    element_cb request_method;  /// 请求方法回调函数指针
    element_cb request_uri;     /// 请求URI回调函数指针
    element_cb fragment;        /// Fragment回调函数指针
    element_cb request_path;    /// 请求路径回调函数指针
    element_cb query_string;    /// 查询字符串回调函数指针
    element_cb http_version;    /// HTTP版本回调函数指针
    element_cb header_done;     /// HTTP头部解析完成回调函数指针

} http_parser;

/**
 * @brief 初始化HTTP解析器
 * @param[out] parser HTTP解析器结构体指针
 * @return 初始化结果，成功返回1，失败返回0
 */
int http_parser_init(http_parser* parser);

/**
 * @brief 完成HTTP解析
 * @param[in,out] parser HTTP解析器结构体指针
 * @return 完成结果，成功返回1，失败返回0
 */
int http_parser_finish(http_parser* parser);

/**
 * @brief 执行HTTP解析
 * @param[in,out] parser HTTP解析器结构体指针
 * @param[in] data 待解析的数据
 * @param[in] len 待解析数据的长度
 * @param[in] off 解析起始偏移量
 * @return 实际解析的字节数
 */
size_t http_parser_execute(http_parser* parser, const char* data, size_t len, size_t off);

/**
 * @brief 检查解析过程中是否有错误
 * @param[in] parser HTTP解析器结构体指针
 * @return 有错误返回非0值，无错误返回0
 */
int http_parser_has_error(http_parser* parser);

/**
 * @brief 检查解析是否已完成
 * @param[in] parser HTTP解析器结构体指针
 * @return 已完成返回非0值，未完成返回0
 */
int http_parser_is_finished(http_parser* parser);

/**
 * @brief 获取已读取的字节数
 * @param[in] parser HTTP解析器结构体指针
 * @return 已读取的字节数
 */
#define http_parser_nread(parser) (parser)->nread