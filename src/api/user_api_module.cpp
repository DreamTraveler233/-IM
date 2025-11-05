#include "api/user_api_module.hpp"

#include "app/user_service.hpp"
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
        dispatch->addServlet("/api/v1/user/detail",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr) {
                                 CIM_LOG_DEBUG(g_logger) << "/api/v1/user/detail";
                                 /*设置响应头*/
                                 res->setHeader("Content-Type", "application/json");

                                 uint64_t uid = GetUidFromToken(req, res);
                                 if (uid == 0) {
                                     return 0;
                                 }

                                 /*根据用户 ID 加载用户信息*/
                                 auto result = CIM::app::UserService::LoadUser(uid);
                                 if (!result.ok) {
                                     res->setStatus(CIM::http::HttpStatus::NOT_FOUND);
                                     res->setBody(Error(404, result.err));
                                     return 0;
                                 }

                                 /*构造响应并返回*/
                                 Json::Value data;
                                 data["id"] = result.user.id;              // 用户ID
                                 data["mobile"] = result.user.mobile;      // 手机号
                                 data["nickname"] = result.user.nickname;  // 昵称
                                 data["email"] = result.user.email;        // 邮箱
                                 data["gender"] = result.user.gender;      // 性别
                                 data["motto"] = result.user.motto;        // 个性签名
                                 data["avatar"] = result.user.avatar;      // 头像URL
                                 data["birthday"] = result.user.birthday;  // 生日
                                 res->setBody(Ok(data));
                                 return 0;
                             });

        /*用户信息更新接口*/
        dispatch->addServlet("/api/v1/user/detail-update",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr) {
                                 CIM_LOG_DEBUG(g_logger) << "/api/v1/user/detail-update";
                                 res->setHeader("Content-Type", "application/json");

                                 uint64_t uid = GetUidFromToken(req, res);
                                 if (uid == 0) {
                                     return 0;
                                 }

                                 /*提取请求字段*/
                                 std::string nickname, avatar, motto, birthday;
                                 uint32_t gender;
                                 Json::Value body;
                                 if (CIM::ParseBody(req->getBody(), body)) {
                                     nickname = CIM::JsonUtil::GetString(body, "nickname");
                                     avatar = CIM::JsonUtil::GetString(body, "avatar");
                                     motto = CIM::JsonUtil::GetString(body, "motto");
                                     gender = CIM::JsonUtil::GetUint32(body, "gender");
                                     birthday = CIM::JsonUtil::GetString(body, "birthday");
                                 }

                                 /*更新用户信息*/
                                 if (!CIM::app::UserService::UpdateUserInfo(
                                         uid, nickname, avatar, motto, gender, birthday)) {
                                     res->setStatus(CIM::http::HttpStatus::INTERNAL_SERVER_ERROR);
                                     res->setBody(Error(500, "更新用户信息失败！"));
                                     return 0;
                                 }

                                 res->setBody(Ok());
                                 return 0;
                             });

        /*用户邮箱更新接口*/
        dispatch->addServlet("/api/v1/user/email-update",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr) {
                                 CIM_LOG_DEBUG(g_logger) << "/api/v1/user/detail-update";
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
