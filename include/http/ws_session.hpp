/**
 * @file    ws_session.hpp
 * @brief   WebSocket协议会话与消息帧相关声明，提供服务端/客户端WebSocket消息收发、握手、心跳等功能。
 * @author  DreamTraveler233
 * @date    2025-11-01
 * @note    依赖于config、http_session等基础模块。
 */

#ifndef __CIM_HTTP_WS_SESSION_HPP__
#define __CIM_HTTP_WS_SESSION_HPP__

#include <stdint.h>

#include <memory>
#include <string>

#include "config/config.hpp"
#include "http_session.hpp"

namespace CIM::http {

/**
 * @struct  WSFrameHead
 * @brief   WebSocket协议帧头结构体，映射RFC6455帧格式。
 *
 * 该结构体用于描述WebSocket数据帧的头部信息，包括操作码、分片、掩码、载荷长度等。
 * @note    使用1字节对齐，便于网络传输。
 */
struct WSFrameHead {
    /**
     * @enum    OPCODE
     * @brief   WebSocket帧操作码类型
     */
    enum OPCODE {
        CONTINUE = 0,    ///< 数据分片帧，非首帧
        TEXT_FRAME = 1,  ///< 文本帧，UTF-8编码
        BIN_FRAME = 2,   ///< 二进制帧
        CLOSE = 8,       ///< 关闭连接帧
        PING = 0x9,      ///< PING心跳帧
        PONG = 0xA       ///< PONG心跳响应帧
    };
    uint32_t opcode : 4;   ///< 操作码，见OPCODE
    bool rsv3 : 1;         ///< 扩展位，通常为0
    bool rsv2 : 1;         ///< 扩展位，通常为0
    bool rsv1 : 1;         ///< 扩展位，通常为0
    bool fin : 1;          ///< 是否为消息最后一帧
    uint32_t payload : 7;  ///< 载荷长度（7位，特殊值126/127表示扩展长度）
    bool mask : 1;         ///< 是否有掩码（客户端发服务端必须为1）

    /**
     * @brief   转为可读字符串，便于日志调试
     * @return  帧头信息字符串
     */
    std::string toString() const;
};

/**
 * @class   WSFrameMessage
 * @brief   WebSocket消息帧封装类
 *
 * 封装单条WebSocket消息（含操作码与数据），支持文本、二进制、控制帧等。
 * 适用于服务端和客户端消息收发。
 */
class WSFrameMessage {
   public:
    using ptr = std::shared_ptr<WSFrameMessage>;  ///< 智能指针类型

    /**
     * @brief   构造函数
     * @param   opcode  操作码，见WSFrameHead::OPCODE
     * @param   data    消息内容
     */
    WSFrameMessage(int opcode = 0, const std::string& data = "");

    /**
     * @brief   获取操作码
     * @return  操作码
     */
    int getOpcode() const { return m_opcode; }

    /**
     * @brief   设置操作码
     * @param   v 操作码
     */
    void setOpcode(int v) { m_opcode = v; }

    /**
     * @brief   获取消息内容（只读）
     * @return  消息内容字符串
     */
    const std::string& getData() const { return m_data; }

    /**
     * @brief   获取消息内容（可写）
     * @return  消息内容字符串引用
     */
    std::string& getData() { return m_data; }

    /**
     * @brief   设置消息内容
     * @param   v 消息内容
     */
    void setData(const std::string& v) { m_data = v; }

   private:
    int m_opcode;        ///< 帧操作码
    std::string m_data;  ///< 消息内容
};

/**
 * @class   WSSession
 * @brief   WebSocket协议会话类，继承自HttpSession
 *
 * 管理单个WebSocket连接的生命周期，包括握手、消息收发、心跳、关闭等。
 * 支持服务端和客户端两种模式。
 * @note    线程不安全，需由上层保证并发安全。
 */
class WSSession : public HttpSession {
   public:
    using ptr = std::shared_ptr<WSSession>;  ///< 智能指针类型

    /**
     * @brief   构造函数
     * @param   sock   套接字对象
     * @param   owner  是否拥有socket所有权
     */
    WSSession(Socket::ptr sock, bool owner = true);

    /**
     * @brief   处理WebSocket握手（服务端/客户端）
     * @return  握手成功返回HttpRequest指针，失败返回nullptr
     * @note    仅在连接建立初期调用
     */
    HttpRequest::ptr handleShake();

    /**
     * @brief   接收一条WebSocket消息
     * @return  WSFrameMessage智能指针，失败返回nullptr
     * @note    阻塞直到收到完整消息或出错
     */
    WSFrameMessage::ptr recvMessage();

    /**
     * @brief   发送一条WebSocket消息
     * @param   msg  消息对象
     * @param   fin  是否为消息最后一帧（默认true）
     * @return  实际发送字节数，失败返回负值
     */
    int32_t sendMessage(WSFrameMessage::ptr msg, bool fin = true);

    /**
     * @brief   发送一条WebSocket消息（直接传字符串）
     * @param   msg    消息内容
     * @param   opcode 操作码，默认文本帧
     * @param   fin    是否为消息最后一帧
     * @return  实际发送字节数，失败返回负值
     */
    int32_t sendMessage(const std::string& msg, int32_t opcode = WSFrameHead::TEXT_FRAME,
                        bool fin = true);

    /**
     * @brief   主动发送PING帧
     * @return  发送结果
     */
    int32_t ping();

    /**
     * @brief   主动发送PONG帧
     * @return  发送结果
     */
    int32_t pong();

   private:
    /**
     * @brief   服务端握手处理
     * @return  是否成功
     */
    bool handleServerShake();

    /**
     * @brief   客户端握手处理
     * @return  是否成功
     */
    bool handleClientShake();
};

/**
 * @var g_websocket_message_max_size
 * @brief   WebSocket单条消息最大长度配置项
 * @note    超过该长度的消息将被拒绝，防止内存溢出
 */
extern CIM::ConfigVar<uint32_t>::ptr g_websocket_message_max_size;

/**
 * @brief   从流中接收一条WebSocket消息
 * @param   stream  数据流指针
 * @param   client  是否为客户端模式
 * @return  WSFrameMessage智能指针，失败返回nullptr
 */
WSFrameMessage::ptr WSRecvMessage(Stream* stream, bool client);

/**
 * @brief   发送一条WebSocket消息到流
 * @param   stream  数据流指针
 * @param   msg     消息对象
 * @param   client  是否为客户端模式
 * @param   fin     是否为消息最后一帧
 * @return  实际发送字节数，失败返回负值
 */
int32_t WSSendMessage(Stream* stream, WSFrameMessage::ptr msg, bool client, bool fin);

/**
 * @brief   发送PING帧到流
 * @param   stream  数据流指针
 * @return  发送结果
 */
int32_t WSPing(Stream* stream);

/**
 * @brief   发送PONG帧到流
 * @param   stream  数据流指针
 * @return  发送结果
 */
int32_t WSPong(Stream* stream);

/**
 * @brief   发送关闭帧到流（可带状态码与原因）
 * @param   stream  数据流指针
 * @param   code    关闭状态码（如1000正常关闭、1002协议错误）
 * @param   reason  关闭原因（可选，UTF-8）
 * @return  发送结果
 */
int32_t WSClose(Stream* stream, uint16_t code = 1000, const std::string& reason = "");

}  // namespace CIM::http

#endif  // __CIM_HTTP_WS_SESSION_HPP__