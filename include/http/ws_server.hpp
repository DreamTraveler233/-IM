/**
 * @file    ws_server.hpp
 * @brief   WebSocket服务端主类声明，负责监听、连接管理与事件分发。
 * @author  DreamTraveler233
 * @date    2025-11-01
 * @note    依赖tcp_server、ws_servlet、ws_session等模块。
 */

#ifndef __CIM_HTTP_WS_SERVER_HPP__
#define __CIM_HTTP_WS_SERVER_HPP__

#include "net/tcp_server.hpp"
#include "ws_servlet.hpp"
#include "ws_session.hpp"

namespace CIM::http {

/**
 * @class   WSServer
 * @brief   WebSocket服务端主类，继承自TcpServer
 *
 * 负责WebSocket协议的监听、连接接入、事件分发。
 * 支持多线程高并发，业务事件通过WSServletDispatch分发。
 * @note    线程安全，适合生产环境。
 */
class WSServer : public TcpServer {
   public:
    using ptr = std::shared_ptr<WSServer>;  ///< 智能指针类型

    /**
     * @brief   构造函数
     * @param   worker         主IO调度器
     * @param   io_worker      读写IO调度器
     * @param   accept_worker  连接接收调度器
     * @note    支持自定义线程池分离，提升性能
     */
    WSServer(IOManager* worker = IOManager::GetThis(), IOManager* io_worker = IOManager::GetThis(),
             IOManager* accept_worker = IOManager::GetThis());

    /**
     * @brief   获取WebSocket业务分发器
     * @return  WSServletDispatch智能指针
     */
    WSServletDispatch::ptr getWSServletDispatch() const { return m_dispatch; }

    /**
     * @brief   设置WebSocket业务分发器
     * @param   v 分发器对象
     */
    void setWSServletDispatch(WSServletDispatch::ptr v) { m_dispatch = v; }

   protected:
    /**
     * @brief   处理新接入的客户端连接
     * @param   client  客户端Socket对象
     * @note    内部完成握手、事件分发、会话管理
     */
    virtual void handleClient(Socket::ptr client) override;

   protected:
    WSServletDispatch::ptr m_dispatch;  ///< WebSocket业务分发器
};

}  // namespace CIM::http

#endif  // __CIM_HTTP_WS_SERVER_HPP__