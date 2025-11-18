#ifndef __CIM_HTTP_HTTP_SERVER_HPP__
#define __CIM_HTTP_HTTP_SERVER_HPP__

#include "http_servlet.hpp"
#include "http_session.hpp"
#include "net/tcp_server.hpp"

namespace CIM::http {
/**
     * @brief HTTP服务器类
     */
class HttpServer : public TcpServer {
   public:
    /// 智能指针类型
    using ptr = std::shared_ptr<HttpServer>;

    /**
         * @brief 构造函数
         * @param[in] keepalive 是否长连接
         * @param[in] worker 工作调度器
         * @param[in] accept_worker 接收连接调度器
         */
    HttpServer(bool keepalive = false, IOManager* worker = IOManager::GetThis(),
               IOManager* io_worker = IOManager::GetThis(),
               IOManager* accept_worker = IOManager::GetThis());

    /**
         * @brief 获取ServletDispatch
         */
    ServletDispatch::ptr getServletDispatch() const { return m_dispatch; }

    /**
         * @brief 设置ServletDispatch
         */
    void setServletDispatch(ServletDispatch::ptr v) { m_dispatch = v; }

    virtual void setName(const std::string& v) override;

   protected:
    virtual void handleClient(Socket::ptr client) override;

   private:
    /// 是否支持长连接
    bool m_isKeepalive;
    /// Servlet分发器
    ServletDispatch::ptr m_dispatch;
};

}  // namespace CIM::http

#endif