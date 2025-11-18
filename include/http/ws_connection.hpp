
/**
 * @file    ws_connection.hpp
 * @brief   WebSocket客户端连接类声明，支持主动握手、消息收发、心跳等。
 * @author  DreamTraveler233
 * @date    2025-11-01
 * @note    依赖http_connection、ws_session等模块。
 */

#ifndef __CIM_HTTP_WS_CONNECTION_HPP__
#define __CIM_HTTP_WS_CONNECTION_HPP__

#include "http_connection.hpp"
#include "ws_session.hpp"

namespace CIM::http {

/**
 * @class   WSConnection
 * @brief   WebSocket客户端连接类，继承自HttpConnection
 *
 * 支持主动发起WebSocket握手、收发消息、协议级心跳等。
 * 适用于客户端/测试工具/服务间通信等场景。
 * @note    线程不安全，需由上层保证并发安全。
 */
class WSConnection : public HttpConnection {
   public:
    using ptr = std::shared_ptr<WSConnection>;  ///< 智能指针类型

    /**
     * @brief   构造函数
     * @param   sock   已连接的Socket对象
     * @param   owner  是否拥有socket所有权
     */
    WSConnection(Socket::ptr sock, bool owner = true);

    /**
     * @brief   通过URL发起WebSocket握手并建立连接
     * @param   url         ws/wss/http/https格式的URL
     * @param   timeout_ms  超时时间（毫秒）
     * @param   headers     额外HTTP头
     * @return  (HttpResult, WSConnection)对，失败时WSConnection为nullptr
     * @note    支持自定义头部（如token、cookie等）
     */
    static std::pair<HttpResult::ptr, WSConnection::ptr> Create(
        const std::string& url, uint64_t timeout_ms,
        const std::map<std::string, std::string>& headers = {});

    /**
     * @brief   通过Uri对象发起WebSocket握手并建立连接
     * @param   uri         Uri对象
     * @param   timeout_ms  超时时间（毫秒）
     * @param   headers     额外HTTP头
     * @return  (HttpResult, WSConnection)对，失败时WSConnection为nullptr
     */
    static std::pair<HttpResult::ptr, WSConnection::ptr> Create(
        Uri::ptr uri, uint64_t timeout_ms, const std::map<std::string, std::string>& headers = {});

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
};

}  // namespace CIM::http

#endif  // __CIM_HTTP_WS_CONNECTION_HPP__