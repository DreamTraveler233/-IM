#include "rock/rock_server.hpp"

#include "base/macro.hpp"
#include "other/module.hpp"

namespace CIM {
static Logger::ptr g_logger = CIM_LOG_NAME("system");

RockServer::RockServer(const std::string& type, IOManager* worker, IOManager* io_worker,
                       IOManager* accept_worker)
    : TcpServer(worker, io_worker, accept_worker) {
    m_type = type;
}

void RockServer::handleClient(Socket::ptr client) {
    CIM_LOG_DEBUG(g_logger) << "handleClient " << *client;
    RockSession::ptr session(new RockSession(client));
    session->setWorker(m_worker);
    ModuleMgr::GetInstance()->foreach (Module::ROCK,
                                       [session](Module::ptr m) { m->onConnect(session); });
    session->setDisconnectCb([](AsyncSocketStream::ptr stream) {
        ModuleMgr::GetInstance()->foreach (Module::ROCK,
                                           [stream](Module::ptr m) { m->onDisconnect(stream); });
    });
    session->setRequestHandler(
        [](RockRequest::ptr req, RockResponse::ptr rsp, RockStream::ptr conn) -> bool {
            // CIM_LOG_INFO(g_logger) << "handleReq " << req->toString()
            //                          << " body=" << req->getBody();
            bool rt = false;
            ModuleMgr::GetInstance()->foreach (Module::ROCK, [&rt, req, rsp, conn](Module::ptr m) {
                if (rt) {
                    return;
                }
                rt = m->handleRequest(req, rsp, conn);
            });
            return rt;
        });
    session->setNotifyHandler([](RockNotify::ptr nty, RockStream::ptr conn) -> bool {
        CIM_LOG_INFO(g_logger) << "handleNty " << nty->toString() << " body=" << nty->getBody();
        bool rt = false;
        ModuleMgr::GetInstance()->foreach (Module::ROCK, [&rt, nty, conn](Module::ptr m) {
            if (rt) {
                return;
            }
            rt = m->handleNotify(nty, conn);
        });
        return rt;
    });
    session->start();
}

}  // namespace CIM