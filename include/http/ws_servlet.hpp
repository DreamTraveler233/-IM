/**
 * @file    ws_servlet.hpp
 * @brief   WebSocket服务端Servlet分发与回调接口声明，支持事件驱动的WS业务开发。
 * @author  DreamTraveler233
 * @date    2025-11-01
 * @note    提供WebSocket消息、连接、关闭等事件的多路由分发与业务扩展能力。
 */

#ifndef __CIM_HTTP_WS_SERVLET_HPP__
#define __CIM_HTTP_WS_SERVLET_HPP__

#include "http_servlet.hpp"
#include "io/thread.hpp"
#include "ws_session.hpp"

namespace CIM::http {

/**
 * @class   WSServlet
 * @brief   WebSocket业务处理抽象基类
 *
 * 作为所有WebSocket业务Servlet的基类，定义了连接建立、关闭、消息处理等核心接口。
 * 业务开发者可继承本类实现自定义事件处理。
 * @note    生命周期由ServletDispatch管理。
 */
class WSServlet : public Servlet {
   public:
    using ptr = std::shared_ptr<WSServlet>;  ///< 智能指针类型

    /**
     * @brief   构造函数
     * @param   name  servlet名称
     */
    WSServlet(const std::string& name) : Servlet(name) {}

    /**
     * @brief   虚析构，便于继承
     */
    virtual ~WSServlet() {}

    /**
     * @brief   兼容HttpServlet接口（无实际意义，WS场景下不调用）
     * @param   request   HTTP请求对象
     * @param   response  HTTP响应对象
     * @param   session   HTTP会话对象
     * @return  固定返回0
     */
    virtual int32_t handle(CIM::http::HttpRequest::ptr request,
                           CIM::http::HttpResponse::ptr response,
                           CIM::http::HttpSession::ptr session) override {
        return 0;
    }

    /**
     * @brief   连接建立事件回调（必须实现）
     * @param   header   握手请求头
     * @param   session  WebSocket会话对象
     * @return  0表示成功，非0表示拒绝连接
     * @note    返回非0将导致连接被关闭
     */
    virtual int32_t onConnect(CIM::http::HttpRequest::ptr header,
                              CIM::http::WSSession::ptr session) = 0;

    /**
     * @brief   连接关闭事件回调（必须实现）
     * @param   header   握手请求头
     * @param   session  WebSocket会话对象
     * @return  0表示正常
     */
    virtual int32_t onClose(CIM::http::HttpRequest::ptr header,
                            CIM::http::WSSession::ptr session) = 0;

    /**
     * @brief   消息处理事件回调（必须实现）
     * @param   header   握手请求头
     * @param   msg      WebSocket消息对象
     * @param   session  WebSocket会话对象
     * @return  0表示正常，非0将关闭连接
     */
    virtual int32_t handle(CIM::http::HttpRequest::ptr header, CIM::http::WSFrameMessage::ptr msg,
                           CIM::http::WSSession::ptr session) = 0;

    /**
     * @brief   获取Servlet名称
     * @return  名称字符串
     */
    const std::string& getName() const { return m_name; }

   protected:
    std::string m_name;  ///< servlet名称
};

/**
 * @class   FunctionWSServlet
 * @brief   基于函数对象的WebSocket业务Servlet
 *
 * 支持以lambda/函数方式快速注册WS事件回调，便于灵活扩展。
 * 适合简单业务或动态路由场景。
 */
class FunctionWSServlet : public WSServlet {
   public:
    using ptr = std::shared_ptr<FunctionWSServlet>;  ///< 智能指针类型
    using on_connect_cb = std::function<int32_t(HttpRequest::ptr header,
                                                WSSession::ptr session)>;  ///< 连接回调类型
    using on_close_cb = std::function<int32_t(HttpRequest::ptr header,
                                              WSSession::ptr session)>;  ///< 关闭回调类型
    using callback = std::function<int32_t(HttpRequest::ptr header, WSFrameMessage::ptr msg,
                                           WSSession::ptr session)>;  ///< 消息回调类型

    /**
     * @brief   构造函数
     * @param   cb          消息回调
     * @param   connect_cb  连接回调
     * @param   close_cb    关闭回调
     */
    FunctionWSServlet(callback cb, on_connect_cb connect_cb = nullptr,
                      on_close_cb close_cb = nullptr);

    /**
     * @brief   连接建立事件回调实现
     * @param   header   握手请求头
     * @param   session  WebSocket会话对象
     * @return  0表示成功，非0表示拒绝连接
     */
    int32_t onConnect(HttpRequest::ptr header, WSSession::ptr session) override;

    /**
     * @brief   连接关闭事件回调实现
     * @param   header   握手请求头
     * @param   session  WebSocket会话对象
     * @return  0表示正常
     */
    int32_t onClose(HttpRequest::ptr header, WSSession::ptr session) override;

    /**
     * @brief   消息处理事件回调实现
     * @param   header   握手请求头
     * @param   msg      WebSocket消息对象
     * @param   session  WebSocket会话对象
     * @return  0表示正常，非0将关闭连接
     */
    int32_t handle(HttpRequest::ptr header, WSFrameMessage::ptr msg,
                   WSSession::ptr session) override;

   protected:
    callback m_callback;        ///< 消息回调
    on_connect_cb m_onConnect;  ///< 连接回调
    on_close_cb m_onClose;      ///< 关闭回调
};

/**
 * @class   WSServletDispatch
 * @brief   WebSocket业务路由分发器
 *
 * 支持按路径注册/查找不同的WebSocket业务Servlet，支持精确与通配路由。
 * 线程安全，适合多线程高并发场景。
 */
class WSServletDispatch : public ServletDispatch {
   public:
    using ptr = std::shared_ptr<WSServletDispatch>;  ///< 智能指针类型
    using RWMutexType = RWMutex;                     ///< 读写锁类型

    /**
     * @brief   构造函数
     */
    WSServletDispatch();

    /**
     * @brief   注册精确路径的WebSocket业务Servlet
     * @param   uri         路径
     * @param   cb          消息回调
     * @param   connect_cb  连接回调
     * @param   close_cb    关闭回调
     */
    void addServlet(const std::string& uri, FunctionWSServlet::callback cb,
                    FunctionWSServlet::on_connect_cb connect_cb = nullptr,
                    FunctionWSServlet::on_close_cb close_cb = nullptr);

    /**
     * @brief   注册通配路径的WebSocket业务Servlet
     * @param   uri         路径（支持*通配）
     * @param   cb          消息回调
     * @param   connect_cb  连接回调
     * @param   close_cb    关闭回调
     */
    void addGlobServlet(const std::string& uri, FunctionWSServlet::callback cb,
                        FunctionWSServlet::on_connect_cb connect_cb = nullptr,
                        FunctionWSServlet::on_close_cb close_cb = nullptr);

    /**
     * @brief   根据路径查找匹配的WebSocket业务Servlet
     * @param   uri 路径
     * @return  匹配的WSServlet智能指针，未找到返回nullptr
     */
    WSServlet::ptr getWSServlet(const std::string& uri);
};

}  // namespace CIM::http

#endif  // __CIM_HTTP_WS_SERVLET_HPP__