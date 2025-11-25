#include "api/emoticon_api_module.hpp"

#include "base/macro.hpp"
#include "common/common.hpp"
#include "http/http_server.hpp"
#include "http/http_servlet.hpp"
#include "system/application.hpp"
#include "util/util.hpp"

namespace IM::api {

static auto g_logger = IM_LOG_NAME("root");

EmoticonApiModule::EmoticonApiModule() : Module("api.emoticon", "0.1.0", "builtin") {}

bool EmoticonApiModule::onServerReady() {
    std::vector<IM::TcpServer::ptr> httpServers;
    if (!IM::Application::GetInstance()->getServer("http", httpServers)) {
        IM_LOG_WARN(g_logger) << "no http servers found when registering emoticon routes";
        return true;
    }

    for (auto& s : httpServers) {
        auto http = std::dynamic_pointer_cast<IM::http::HttpServer>(s);
        if (!http) continue;
        auto dispatch = http->getServletDispatch();

        dispatch->addServlet(
            "/api/v1/emoticon/customize/create",
            [this](IM::http::HttpRequest::ptr /*req*/, IM::http::HttpResponse::ptr res,
                   IM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                res->setBody(Ok());
                return 0;
            });
        dispatch->addServlet(
            "/api/v1/emoticon/customize/delete",
            [this](IM::http::HttpRequest::ptr /*req*/, IM::http::HttpResponse::ptr res,
                   IM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                res->setBody(Ok());
                return 0;
            });
        dispatch->addServlet(
            "/api/v1/emoticon/customize/list",
            [this](IM::http::HttpRequest::ptr /*req*/, IM::http::HttpResponse::ptr res,
                   IM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                Json::Value d;
                d["list"] = Json::Value(Json::arrayValue);
                res->setBody(Ok(d));
                return 0;
            });
        dispatch->addServlet(
            "/api/v1/emoticon/customize/upload",
            [this](IM::http::HttpRequest::ptr /*req*/, IM::http::HttpResponse::ptr res,
                   IM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                res->setBody(Ok());
                return 0;
            });
    }
    return true;
}

}  // namespace IM::api
