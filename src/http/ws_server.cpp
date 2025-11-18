#include "http/ws_server.hpp"

#include "base/macro.hpp"

namespace CIM::http {
static auto g_logger = CIM_LOG_NAME("system");

WSServer::WSServer(CIM::IOManager* worker, CIM::IOManager* io_worker, CIM::IOManager* accept_worker)
    : TcpServer(worker, io_worker, accept_worker) {
    m_dispatch.reset(new WSServletDispatch);
    m_type = "websocket_server";
}

void WSServer::handleClient(Socket::ptr client) {
    CIM_LOG_DEBUG(g_logger) << "handleClient " << *client;
    // 创建WebSocket会话对象，封装底层socket
    WSSession::ptr session(new WSSession(client));
    do {
        // 1. 进行WebSocket握手，获取HTTP请求头
        HttpRequest::ptr header = session->handleShake();
        if (!header) {
            // 握手失败，直接退出
            CIM_LOG_DEBUG(g_logger) << "handleShake error";
            break;
        }
        // 2. 路由分发，查找对应的业务Servlet
        WSServlet::ptr servlet = m_dispatch->getWSServlet(header->getPath());
        if (!servlet) {
            // 未找到匹配Servlet，关闭连接
            CIM_LOG_DEBUG(g_logger) << "no match WSServlet";
            break;
        }
        // 3. 连接建立事件回调（如鉴权、会话登记等）
        int rt = servlet->onConnect(header, session);
        if (rt) {
            // 回调返回非0，拒绝连接
            CIM_LOG_DEBUG(g_logger) << "onConnect return " << rt;
            break;
        }
        // 4. 消息主循环，持续接收并分发消息
        while (true) {
            auto msg = session->recvMessage();
            if (!msg) {
                // 连接断开或异常，跳出循环
                break;
            }
            // 业务消息处理回调
            rt = servlet->handle(header, msg, session);
            if (rt) {
                // 回调返回非0，关闭连接
                CIM_LOG_DEBUG(g_logger) << "handle return " << rt;
                break;
            }
        }
        // 5. 连接关闭事件回调（如资源清理、日志等）
        servlet->onClose(header, session);
    } while (0);
    // 6. 关闭底层会话，释放资源
    session->close();
}
}  // namespace CIM::http