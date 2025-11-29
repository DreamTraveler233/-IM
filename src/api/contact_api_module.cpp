#include "api/contact_api_module.hpp"

#include "api/ws_gateway_module.hpp"
#include "base/macro.hpp"
#include "common/common.hpp"
#include "http/http_server.hpp"
#include "http/http_servlet.hpp"
#include "system/application.hpp"
#include "util/util.hpp"

namespace IM::api {

static auto g_logger = IM_LOG_NAME("root");

ContactApiModule::ContactApiModule(IM::domain::service::IContactService::Ptr contact_service,
                                   IM::domain::service::IUserService::Ptr user_service)
    : Module("api.contact", "0.1.0", "builtin"),
      m_contact_service(std::move(contact_service)),
      m_user_service(std::move(user_service)) {}

bool ContactApiModule::onServerReady() {
    std::vector<IM::TcpServer::ptr> httpServers;
    if (!IM::Application::GetInstance()->getServer("http", httpServers)) {
        IM_LOG_WARN(g_logger) << "no http servers found when registering contact routes";
        return true;
    }

    for (auto& s : httpServers) {
        auto http = std::dynamic_pointer_cast<IM::http::HttpServer>(s);
        if (!http) continue;
        auto dispatch = http->getServletDispatch();

        /*同意联系人申请接口*/
        dispatch->addServlet("/api/v1/contact-apply/accept",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");

                                 uint64_t apply_id;
                                 std::string remark;
                                 Json::Value body;
                                 if (ParseBody(req->getBody(), body)) {
                                     apply_id = IM::JsonUtil::GetUint64(body, "apply_id");
                                     remark = IM::JsonUtil::GetString(body, "remark");
                                 }

                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }

                                 /*调用业务逻辑处理接受好友申请（服务端已创建会话并返回）*/
                                 auto session_result = m_contact_service->AgreeApply(
                                     uid_result.data, apply_id, remark);
                                 if (!session_result.ok) {
                                     res->setStatus(ToHttpStatus(session_result.code));
                                     res->setBody(Error(session_result.code, session_result.err));
                                     return 0;
                                 }

                                 Json::Value d;
                                 if (session_result.data.id != 0) {
                                     Json::Value s;
                                     s["id"] = (Json::UInt64)session_result.data.id;
                                     s["talk_mode"] = (int)session_result.data.talk_mode;
                                     s["to_from_id"] = (Json::UInt64)session_result.data.to_from_id;
                                     s["is_top"] = (int)session_result.data.is_top;
                                     s["is_disturb"] = (int)session_result.data.is_disturb;
                                     s["is_robot"] = (int)session_result.data.is_robot;
                                     s["name"] = session_result.data.name;
                                     s["avatar"] = session_result.data.avatar;
                                     s["remark"] = session_result.data.remark;
                                     s["unread_num"] = (int)session_result.data.unread_num;
                                     s["msg_text"] = session_result.data.msg_text;
                                     s["updated_at"] = session_result.data.updated_at;
                                     d["session"] = s;
                                 }
                                 res->setBody(Ok(d));
                                 return 0;
                             });

        /*添加联系人申请接口*/
        dispatch->addServlet("/api/v1/contact-apply/create",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");

                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }

                                 /*提取请求字段*/
                                 uint64_t to_id;
                                 std::string remark;
                                 Json::Value body;
                                 if (ParseBody(req->getBody(), body)) {
                                     to_id = IM::JsonUtil::GetUint64(body, "user_id");
                                     remark = IM::JsonUtil::GetString(body, "remark");
                                 }

                                 /*调用业务逻辑处理添加联系人申请*/
                                 auto result = m_contact_service->CreateContactApply(
                                     uid_result.data, to_id, remark);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }

                                 res->setBody(Ok());
                                 return 0;
                             });

        /*拒绝联系人申请接口*/
        dispatch->addServlet("/api/v1/contact-apply/decline",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");

                                 uint64_t apply_id;
                                 std::string remark;
                                 Json::Value body;
                                 if (ParseBody(req->getBody(), body)) {
                                     apply_id = IM::JsonUtil::GetUint64(body, "apply_id");
                                     remark = IM::JsonUtil::GetString(body, "remark");
                                 }

                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }

                                 /*调用业务逻辑处理接受好友申请*/
                                 auto result = m_contact_service->RejectApply(uid_result.data,
                                                                              apply_id, remark);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }

                                 res->setBody(Ok());
                                 return 0;
                             });

        /*获取好友申请列表接口*/
        dispatch->addServlet("/api/v1/contact-apply/list",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");

                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }

                                 /*获取未处理的好友申请列表*/
                                 auto result =
                                     m_contact_service->ListContactApplies(uid_result.data);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }

                                 Json::Value d;
                                 Json::Value items;
                                 for (const auto& item : result.data) {
                                     Json::Value jitem;
                                     jitem["id"] = item.id;
                                     jitem["user_id"] = item.target_user_id;
                                     jitem["friend_id"] = item.apply_user_id;
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

        /*获取未读好友申请数量接口*/
        dispatch->addServlet(
            "/api/v1/contact-apply/unread-num",
            [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                   IM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");

                auto uid_result = GetUidFromToken(req, res);
                if (!uid_result.ok) {
                    res->setStatus(ToHttpStatus(uid_result.code));
                    res->setBody(Error(uid_result.code, uid_result.err));
                    return 0;
                }

                auto result = m_contact_service->GetPendingContactApplyCount(uid_result.data);
                if (!result.ok) {
                    res->setStatus(ToHttpStatus(result.code));
                    res->setBody(Error(result.code, result.err));
                    return 0;
                }

                Json::Value data;
                data["num"] = result.data;
                res->setBody(Ok(data));
                return 0;
            });

        /*获取联系人分组列表接口*/
        dispatch->addServlet("/api/v1/contact-group/list",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");

                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }

                                 /*调用业务逻辑处理获取联系人分组列表*/
                                 auto result =
                                     m_contact_service->GetContactGroupLists(uid_result.data);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }

                                 Json::Value items;
                                 Json::Value data;
                                 for (const auto& item : result.data) {
                                     Json::Value jitem;
                                     jitem["id"] = item.id;
                                     jitem["name"] = item.name;
                                     jitem["sort"] = item.sort;
                                     jitem["count"] = item.contact_count;
                                     data.append(jitem);
                                 }
                                 items["items"] = data;
                                 res->setBody(Ok(items));
                                 return 0;
                             });

        /*保存联系人分组接口*/
        dispatch->addServlet(
            "/api/v1/contact-group/save",
            [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                   IM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");

                std::vector<std::tuple<uint64_t, uint64_t, std::string>> groupItems;
                Json::Value body;
                if (ParseBody(req->getBody(), body)) {
                    auto items = body["items"];
                    for (const auto& item : items) {
                        uint64_t id = JsonUtil::GetUint64(item, "id");
                        uint64_t sort = JsonUtil::GetUint64(item, "sort");
                        std::string name = JsonUtil::GetString(item, "name");
                        groupItems.emplace_back(id, sort, name);
                    }
                }

                auto uid_result = GetUidFromToken(req, res);
                if (!uid_result.ok) {
                    res->setStatus(ToHttpStatus(uid_result.code));
                    res->setBody(Error(uid_result.code, uid_result.err));
                    return 0;
                }

                /*调用业务逻辑处理修改联系人分组*/
                auto result = m_contact_service->SaveContactGroup(uid_result.data, groupItems);
                if (!result.ok) {
                    res->setStatus(ToHttpStatus(result.code));
                    res->setBody(Error(result.code, result.err));
                    return 0;
                }

                res->setBody(Ok());
                return 0;
            });

        /*修改联系人分组接口*/
        dispatch->addServlet("/api/v1/contact/change-group",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");

                                 uint64_t contact_id, group_id;
                                 Json::Value body;
                                 if (ParseBody(req->getBody(), body)) {
                                     contact_id = IM::JsonUtil::GetUint64(body, "user_id");
                                     group_id = IM::JsonUtil::GetUint64(body, "group_id");
                                 }

                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }

                                 auto result = m_contact_service->ChangeContactGroup(
                                     uid_result.data, contact_id, group_id);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }

                                 res->setBody(Ok());
                                 return 0;
                             });

        /*删除联系人接口*/
        dispatch->addServlet("/api/v1/contact/delete",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");

                                 uint64_t contact_id;
                                 Json::Value body;
                                 if (ParseBody(req->getBody(), body)) {
                                     contact_id = IM::JsonUtil::GetUint64(body, "user_id");
                                 }

                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }

                                 /*调用业务逻辑处理删除联系人*/
                                 auto result =
                                     m_contact_service->DeleteContact(uid_result.data, contact_id);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });

        /*获取联系人详情接口*/
        dispatch->addServlet("/api/v1/contact/detail",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");

                                 uint64_t target_id;
                                 Json::Value body;
                                 if (ParseBody(req->getBody(), body)) {
                                     target_id = IM::JsonUtil::GetUint64(body, "user_id");
                                 }

                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }

                                 auto result = m_contact_service->GetContactDetail(uid_result.data,
                                                                                   target_id);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
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
                                 if (uid_result.data == target_id) {
                                     user["relation"] = 4;  // 自己
                                 } else {
                                     user["relation"] = result.data.relation;
                                 }
                                 user["contact_remark"] = result.data.contact_remark;
                                 user["contact_group_id"] = result.data.contact_group_id;

                                 res->setBody(Ok(user));
                                 return 0;
                             });

        /*编辑联系人备注接口*/
        dispatch->addServlet("/api/v1/contact/edit-remark",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");

                                 uint64_t contact_id;
                                 std::string remark;
                                 Json::Value body;
                                 if (ParseBody(req->getBody(), body)) {
                                     contact_id = IM::JsonUtil::GetUint64(body, "user_id");
                                     remark = IM::JsonUtil::GetString(body, "remark");
                                 }

                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }

                                 /*调用业务逻辑处理编辑联系人备注*/
                                 auto result = m_contact_service->EditContactRemark(
                                     uid_result.data, contact_id, remark);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }

                                 res->setBody(Ok());
                                 return 0;
                             });

        /*获取联系人列表接口*/
        dispatch->addServlet("/api/v1/contact/list",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");

                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }

                                 /*根据用户 ID 获取用户好友列表*/
                                 auto contacts = m_contact_service->ListFriends(uid_result.data);
                                 if (!contacts.ok) {
                                     res->setStatus(ToHttpStatus(contacts.code));
                                     res->setBody(Error(contacts.code, contacts.err));
                                     return 0;
                                 }

                                 Json::Value d;
                                 Json::Value items;
                                 for (const auto& c : contacts.data) {
                                     Json::Value item;
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

        /*获取联系人在线状态接口*/
        dispatch->addServlet("/api/v1/contact/online-status",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");

                                 uint64_t contact_id;
                                 Json::Value body;
                                 if (ParseBody(req->getBody(), body)) {
                                     contact_id = IM::JsonUtil::GetUint64(body, "user_id");
                                 }

                                 auto result = m_user_service->GetUserOnlineStatus(contact_id);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }

                                 Json::Value data;
                                 data["online_status"] = result.data;
                                 res->setBody(Ok(data));
                                 return 0;
                             });

        /*搜索联系人接口*/
        dispatch->addServlet("/api/v1/contact/search",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");

                                 std::string mobile;
                                 Json::Value body;
                                 if (ParseBody(req->getBody(), body)) {
                                     mobile = IM::JsonUtil::GetString(body, "mobile");
                                 }

                                 auto result = m_contact_service->SearchByMobile(mobile);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
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
    return true;
}

}  // namespace IM::api
