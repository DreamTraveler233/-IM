#include "api/contact_api_module.hpp"

#include "app/contact_service.hpp"
#include "base/macro.hpp"
#include "common/common.hpp"
#include "http/http_server.hpp"
#include "http/http_servlet.hpp"
#include "system/application.hpp"
#include "util/util.hpp"

namespace CIM::api {

static auto g_logger = CIM_LOG_NAME("root");

ContactApiModule::ContactApiModule() : Module("api.contact", "0.1.0", "builtin") {}

bool ContactApiModule::onServerReady() {
    std::vector<CIM::TcpServer::ptr> httpServers;
    if (!CIM::Application::GetInstance()->getServer("http", httpServers)) {
        CIM_LOG_WARN(g_logger) << "no http servers found when registering contact routes";
        return true;
    }

    for (auto& s : httpServers) {
        auto http = std::dynamic_pointer_cast<CIM::http::HttpServer>(s);
        if (!http) continue;
        auto dispatch = http->getServletDispatch();

        /*同意联系人申请接口*/
        dispatch->addServlet("/api/v1/contact-apply/accept",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");

                                 uint64_t apply_id;
                                 std::string remark;
                                 Json::Value body;
                                 if (ParseBody(req->getBody(), body)) {
                                     apply_id = CIM::JsonUtil::GetUint64(body, "apply_id");
                                     remark = CIM::JsonUtil::GetString(body, "remark");
                                 }

                                 /*调用业务逻辑处理接受好友申请*/
                                 auto result =
                                     CIM::app::ContactService::AgreeApply(apply_id, remark);
                                 if (!result.ok) {
                                     res->setStatus(CIM::http::HttpStatus::BAD_REQUEST);
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }

                                 res->setBody(Ok());
                                 return 0;
                             });

        /*添加联系人申请接口*/
        dispatch->addServlet("/api/v1/contact-apply/create",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");

                                 /*验证并提取请求中的用户ID*/
                                 uint64_t from_id = GetUidFromToken(req, res);
                                 if (from_id == 0) {
                                     return 0;  // 错误响应已在 GetUidFromToken 中设置
                                 }

                                 /*提取请求字段*/
                                 uint64_t to_id;
                                 std::string remark;
                                 Json::Value body;
                                 if (ParseBody(req->getBody(), body)) {
                                     to_id = CIM::JsonUtil::GetUint64(body, "user_id");
                                     remark = CIM::JsonUtil::GetString(body, "remark");
                                 }

                                 /*调用业务逻辑处理添加联系人申请*/
                                 auto result = CIM::app::ContactService::CreateContactApply(
                                     from_id, to_id, remark);
                                 if (!result.ok) {
                                     res->setStatus(CIM::http::HttpStatus::BAD_REQUEST);
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }

                                 res->setBody(Ok());
                                 return 0;
                             });
        /*拒绝联系人申请接口*/
        dispatch->addServlet("/api/v1/contact-apply/decline",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 uint64_t apply_id;
                                 std::string remark;
                                 Json::Value body;
                                 if (ParseBody(req->getBody(), body)) {
                                     apply_id = CIM::JsonUtil::GetUint64(body, "apply_id");
                                     remark = CIM::JsonUtil::GetString(body, "remark");
                                 }

                                 /*调用业务逻辑处理接受好友申请*/
                                 auto result =
                                     CIM::app::ContactService::RejectApply(apply_id, remark);
                                 if (!result.ok) {
                                     res->setStatus(CIM::http::HttpStatus::BAD_REQUEST);
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }

                                 res->setBody(Ok());
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/contact-apply/list",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr /*session*/) {
                                 CIM_LOG_DEBUG(g_logger) << "/api/v1/contact-apply/list";
                                 res->setHeader("Content-Type", "application/json");

                                 uint64_t uid = GetUidFromToken(req, res);
                                 if (uid == 0) {
                                     return 0;
                                 }

                                 /*获取未处理的好友申请列表*/
                                 auto result = CIM::app::ContactService::ListContactApplies(uid);
                                 if (!result.ok) {
                                     res->setStatus(CIM::http::HttpStatus::INTERNAL_SERVER_ERROR);
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }

                                 Json::Value d;
                                 Json::Value items;
                                 for (const auto& item : result.data) {
                                     Json::Value jitem;
                                     jitem["id"] = item.id;
                                     jitem["user_id"] = item.user_id;
                                     jitem["friend_id"] = item.friend_id;
                                     jitem["remark"] = item.remark;
                                     jitem["nickname"] = item.nickname;
                                     jitem["avatar"] = item.avatar;
                                     jitem["created_at"] = item.created_at;
                                     items.append(jitem);
                                 }
                                 d["items"] = items;
                                 res->setBody(Ok(d));
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/contact-apply/unread-num",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");

                                 uint64_t uid = GetUidFromToken(req, res);
                                 if (uid == 0) {
                                     return 0;
                                 }

                                 auto result =
                                     CIM::app::ContactService::GetPendingContactApplyCount(uid);
                                 if (!result.ok) {
                                     res->setStatus(CIM::http::HttpStatus::INTERNAL_SERVER_ERROR);
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }

                                 Json::Value data;
                                 data["num"] = result.data;
                                 res->setBody(Ok(data));
                                 return 0;
                             });

        // contact-group
        dispatch->addServlet("/api/v1/contact-group/list",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 res->setBody(Ok());
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/contact-group/save",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 res->setBody(Ok());
                                 return 0;
                             });

        // contact
        dispatch->addServlet("/api/v1/contact/change-group",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 res->setBody(Ok());
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/contact/delete",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 res->setBody(Ok());
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/contact/detail", [](CIM::http::HttpRequest::ptr req,
                                                          CIM::http::HttpResponse::ptr res,
                                                          CIM::http::HttpSession::ptr /*session*/) {
            CIM_LOG_DEBUG(g_logger) << "/api/v1/contact/detail";
            res->setHeader("Content-Type", "application/json");

            uint64_t target_id;
            Json::Value body;
            if (ParseBody(req->getBody(), body)) {
                target_id = CIM::JsonUtil::GetUint64(body, "user_id");
            }

            uint64_t owner_id = GetUidFromToken(req, res);
            if (owner_id == 0) {
                return 0;
            }

            auto result = CIM::app::ContactService::GetContactDetail(owner_id, target_id);
            if (!result.ok) {
                res->setStatus(CIM::http::HttpStatus::NOT_FOUND);
                res->setBody(Error(result.code, result.err));
                return 0;
            }

            Json::Value user;
            user["user_id"] = result.data.user_id;
            user["mobile"] = result.data.mobile;
            user["nickname"] = result.data.nickname;
            user["avatar"] = result.data.avatar;
            user["gender"] = result.data.gender;
            user["motto"] = result.data.motto;
            user["email"] = result.data.email;
            if (owner_id == target_id) {
                user["relation"] = 4;  // 自己
            } else {
                user["relation"] = result.data.relation;
            }
            user["contact_remark"] = result.data.contact_remark;
            user["contact_group_id"] = result.data.contact_group_id;

            res->setBody(Ok(user));
            return 0;
        });
        dispatch->addServlet("/api/v1/contact/edit-remark",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 res->setBody(Ok());
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/contact/list",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr /*session*/) {
                                 CIM_LOG_DEBUG(g_logger) << "/api/v1/contact/list";
                                 res->setHeader("Content-Type", "application/json");

                                 uint64_t uid = GetUidFromToken(req, res);
                                 if (uid == 0) {
                                     return 0;
                                 }

                                 /*根据用户 ID 获取用户好友列表*/
                                 auto contacts = CIM::app::ContactService::ListFriends(uid);
                                 if (!contacts.ok) {
                                     res->setStatus(CIM::http::HttpStatus::INTERNAL_SERVER_ERROR);
                                     res->setBody(Error(contacts.code, contacts.err));
                                     return 0;
                                 }

                                 Json::Value d(Json::objectValue);
                                 Json::Value items(Json::arrayValue);
                                 for (const auto& c : contacts.data) {
                                     Json::Value item(Json::objectValue);
                                     item["user_id"] = c.user_id;
                                     item["nickname"] = c.nickname;
                                     item["gender"] = c.gender;
                                     item["motto"] = c.motto;
                                     item["avatar"] = c.avatar;
                                     item["remark"] = c.remark;
                                     item["group_id"] = c.group_id;
                                     items.append(item);
                                 }
                                 d["items"] = items;
                                 res->setBody(Ok(d));
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/contact/online-status",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 Json::Value d;
                                 d["list"] = Json::Value(Json::arrayValue);
                                 res->setBody(Ok(d));
                                 return 0;
                             });

        dispatch->addServlet("/api/v1/contact/search",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr /*session*/) {
                                 CIM_LOG_DEBUG(g_logger) << "/api/v1/contact/search";
                                 res->setHeader("Content-Type", "application/json");

                                 std::string mobile;
                                 Json::Value body;
                                 if (ParseBody(req->getBody(), body)) {
                                     mobile = CIM::JsonUtil::GetString(body, "mobile");
                                 }

                                 auto result = CIM::app::ContactService::SearchByMobile(mobile);
                                 if (!result.ok) {
                                     res->setStatus(CIM::http::HttpStatus::NOT_FOUND);
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }

                                 Json::Value user;
                                 user["user_id"] = result.data.id;
                                 user["mobile"] = result.data.mobile;
                                 user["nickname"] = result.data.nickname;
                                 user["avatar"] = result.data.avatar;
                                 user["gender"] = result.data.gender;
                                 user["motto"] = result.data.motto;
                                 res->setBody(Ok(user));
                                 return 0;
                             });
    }

    CIM_LOG_INFO(g_logger) << "contact routes registered";
    return true;
}

}  // namespace CIM::api
