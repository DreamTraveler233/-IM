#include "api/user_api_module.hpp"

#include "base/macro.hpp"
#include "common/common.hpp"
#include "http/http_server.hpp"
#include "http/http_servlet.hpp"
#include "system/application.hpp"
#include "util/util.hpp"

namespace CIM::api {

static auto g_logger = CIM_LOG_NAME("root");

UserApiModule::UserApiModule() : Module("api.user", "0.1.0", "builtin") {}

bool UserApiModule::onServerReady() {
    std::vector<CIM::TcpServer::ptr> httpServers;
    if (!CIM::Application::GetInstance()->getServer("http", httpServers)) {
        CIM_LOG_WARN(g_logger) << "no http servers found when registering user routes";
        return true;
    }

    for (auto& s : httpServers) {
        auto http = std::dynamic_pointer_cast<CIM::http::HttpServer>(s);
        if (!http) continue;
        auto dispatch = http->getServletDispatch();

        /*用户相关接口*/
        dispatch->addServlet("/api/v1/user/detail", [](CIM::http::HttpRequest::ptr req,
                                                       CIM::http::HttpResponse::ptr res,
                                                       CIM::http::HttpSession::ptr) {
            CIM_LOG_DEBUG(g_logger) << "/api/v1/user/detail";
            /*设置响应头*/
            res->setHeader("Content-Type", "application/json");

            /*加载用户信息*/

            
            /*返回结果*/
            res->setBody(Ok());
            return 0;
        });

        /*用户信息更新接口*/
        dispatch->addServlet("/api/v1/user/detail-update",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr) {
                                 CIM_LOG_DEBUG(g_logger) << "/api/v1/user/detail-update";
                                 res->setHeader("Content-Type", "application/json");
                                 std::string nickname, avatar, motto, gender, birthday;
                                 Json::Value body;
                                 if (CIM::ParseBody(req->getBody(), body)) {
                                     nickname = CIM::JsonUtil::GetString(body, "nickname", "");
                                     avatar = CIM::JsonUtil::GetString(body, "avatar", "");
                                     motto = CIM::JsonUtil::GetString(body, "motto", "");
                                     gender = CIM::JsonUtil::GetString(body, "gender", "");
                                     birthday = CIM::JsonUtil::GetString(body, "birthday", "");
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });

        /*用户邮箱更新接口*/
        dispatch->addServlet("/api/v1/user/email-update",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 res->setBody(Ok());
                                 return 0;
                             });

        /*用户手机更新接口*/
        dispatch->addServlet("/api/v1/user/mobile-update",
                             [](CIM::http::HttpRequest::ptr, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 res->setBody(Ok());
                                 return 0;
                             });

        /*用户密码更新接口*/
        dispatch->addServlet("/api/v1/user/password-update",
                             [](CIM::http::HttpRequest::ptr, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 res->setBody(Ok());
                                 return 0;
                             });

        /*用户设置接口*/
        dispatch->addServlet("/api/v1/user/setting",
                             [](CIM::http::HttpRequest::ptr, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 res->setBody(Ok());
                                 return 0;
                             });
    }

    CIM_LOG_INFO(g_logger) << "user routes registered";
    return true;
}

}  // namespace CIM::api
