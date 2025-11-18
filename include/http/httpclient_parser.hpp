/**
 *
 * Copyright (c) 2010, Zed A. Shaw and Mongrel2 Project Contributors.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *     * Neither the name of the Mongrel2 Project, Zed A. Shaw, nor the names
 *       of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file httpclient_parser.hpp
 * @brief HTTP客户端响应解析器
 *
 * 本文件定义了HTTP客户端响应解析器的核心结构和接口，用于解析HTTP响应数据。
 * 解析器基于Ragel状态机工具生成，能够处理HTTP响应行、状态码、响应头和响应体等信息。
 *
 * 主要功能包括：
 * - HTTP响应状态行解析（版本、状态码、原因短语）
 * - HTTP响应头部字段解析
 * - 分块传输编码（Chunked）支持
 * - 连接关闭标记检测
 *
 * 解析过程通过回调函数通知上层应用解析到的数据，解析器本身不存储解析结果。
 */

#pragma once

#include "http11_common.hpp"

/**
 * @brief HTTP客户端响应解析器结构体
 *
 * 该结构体用于存储HTTP客户端响应解析过程中的状态和数据，
 * 是基于Ragel状态机生成的C代码包装
 */
typedef struct httpclient_parser {
    int cs;              /// 当前状态机状态
    size_t body_start;   /// HTTP消息体开始位置
    int content_len;     /// 内容长度
    int status;          /// HTTP状态码
    int chunked;         /// 是否为分块传输编码
    int chunks_done;     /// 分块是否完成
    int close;           /// 连接是否关闭标记
    size_t nread;        /// 已读取的字节数
    size_t mark;         /// 标记位置，用于记录解析过程中的关键位置
    size_t field_start;  /// 当前HTTP头部字段开始位置
    size_t field_len;    /// 当前HTTP头部字段长度

    void* data;  /// 用户自定义数据指针，用于回调函数传递数据

    field_cb http_field;       /// HTTP头部字段回调函数指针
    element_cb reason_phrase;  /// 原因短语回调函数指针
    element_cb status_code;    /// 状态码回调函数指针
    element_cb chunk_size;     /// 分块大小回调函数指针
    element_cb http_version;   /// HTTP版本回调函数指针
    element_cb header_done;    /// HTTP头部解析完成回调函数指针
    element_cb last_chunk;     /// 最后一个分块回调函数指针

} httpclient_parser;

/**
 * @brief 初始化HTTP客户端解析器
 * @param[out] parser HTTP客户端解析器结构体指针
 * @return 初始化结果，成功返回0，失败返回非0值
 */
int httpclient_parser_init(httpclient_parser* parser);

/**
 * @brief 完成HTTP客户端解析
 * @param[in,out] parser HTTP客户端解析器结构体指针
 * @return 完成结果，成功返回0，失败返回非0值
 */
int httpclient_parser_finish(httpclient_parser* parser);

/**
 * @brief 执行HTTP客户端解析
 * @param[in,out] parser HTTP客户端解析器结构体指针
 * @param[in] data 待解析的数据
 * @param[in] len 待解析数据的长度
 * @param[in] off 解析起始偏移量
 * @return 解析结果，成功返回0，失败返回非0值
 */
int httpclient_parser_execute(httpclient_parser* parser, const char* data, size_t len, size_t off);

/**
 * @brief 检查解析过程中是否有错误
 * @param[in] parser HTTP客户端解析器结构体指针
 * @return 有错误返回非0值，无错误返回0
 */
int httpclient_parser_has_error(httpclient_parser* parser);

/**
 * @brief 检查解析是否已完成
 * @param[in] parser HTTP客户端解析器结构体指针
 * @return 已完成返回非0值，未完成返回0
 */
int httpclient_parser_is_finished(httpclient_parser* parser);

/**
 * @brief 获取已读取的字节数
 * @param[in] parser HTTP客户端解析器结构体指针
 * @return 已读取的字节数
 */
#define httpclient_parser_nread(parser) (parser)->nread