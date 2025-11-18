#include "http/http_server.hpp"

#include "http/servlets/config_servlet.hpp"
#include "base/macro.hpp"
#include "http/servlets/status_servlet.hpp"

namespace CIM::http {

static auto g_logger = CIM_LOG_NAME("system");

HttpServer::HttpServer(bool keepalive, IOManager* worker, IOManager* io_worker,
                       IOManager* accept_worker)
    : TcpServer(worker, io_worker, accept_worker), m_isKeepalive(keepalive) {
    m_dispatch.reset(new ServletDispatch);

    m_type = "http";
    m_dispatch->addServlet("/_/status", Servlet::ptr(new StatusServlet));
    m_dispatch->addServlet("/_/config", Servlet::ptr(new ConfigServlet));
}

void HttpServer::setName(const std::string& v) {
    TcpServer::setName(v);
    m_dispatch->setDefault(std::make_shared<NotFoundServlet>(v));
}

void HttpServer::handleClient(Socket::ptr client) {
    CIM_LOG_DEBUG(g_logger) << "handleClient " << *client;
    /* 创建 HTTP 会话 */
    HttpSession::ptr session(new HttpSession(client));
    do {
        /* 接收 HTTP 请求 */
        CIM_LOG_DEBUG(g_logger) << "waiting for http request from " << *client;
        auto req = session->recvRequest();
        if (!req) {
            CIM_LOG_DEBUG(g_logger)
                << "recv http request fail, errno=" << errno << " errstr=" << strerror(errno)
                << " cliet:" << *client << " keep_alive=" << m_isKeepalive;
            break;
        }

        /* 处理 HTTP 请求 */
        HttpResponse::ptr rsp(
            new HttpResponse(req->getVersion(), req->isClose() || !m_isKeepalive));
        rsp->setHeader("Server", getName());
        m_dispatch->handle(req, rsp, session);  // 路由分发
        session->sendResponse(rsp);             // 发送响应数据

        /* 如果不是长连接或者客户端关闭，则关闭会话 */
        if (!m_isKeepalive || req->isClose()) {
            break;
        }
    } while (true);
    session->close();
}

}  // namespace CIM::http