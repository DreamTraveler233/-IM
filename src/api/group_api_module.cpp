#include "api/group_api_module.hpp"

#include <set>
#include "base/macro.hpp"
#include "common/common.hpp"
#include "http/http_server.hpp"
#include "http/http_servlet.hpp"
#include "system/application.hpp"
#include "util/util.hpp"

namespace IM::api {

static auto g_logger = IM_LOG_NAME("root");

GroupApiModule::GroupApiModule(IM::domain::service::IGroupService::Ptr group_service,
                               IM::domain::service::IContactService::Ptr contact_service)
    : Module("api.group", "0.1.0", "builtin"),
      m_group_service(std::move(group_service)),
      m_contact_service(std::move(contact_service)) {}

bool GroupApiModule::onServerReady() {
    std::vector<IM::TcpServer::ptr> httpServers;
    if (!IM::Application::GetInstance()->getServer("http", httpServers)) {
        IM_LOG_WARN(g_logger) << "no http servers found when registering group routes";
        return true;
    }

    for (auto& s : httpServers) {
        auto http = std::dynamic_pointer_cast<IM::http::HttpServer>(s);
        if (!http) continue;
        auto dispatch = http->getServletDispatch();

        // 群组申请
        dispatch->addServlet("/api/v1/group-apply/agree",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t apply_id = IM::JsonUtil::GetUint64(body, "apply_id");
                                 auto result =
                                     m_group_service->AgreeApply(uid_result.data, apply_id);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });

        dispatch->addServlet("/api/v1/group-apply/all",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 auto result = m_group_service->GetUserApplyList(uid_result.data);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 Json::Value d;
                                 Json::Value list(Json::arrayValue);
                                 for (const auto& item : result.data) {
                                     Json::Value v;
                                     v["id"] = (Json::UInt64)item.id;
                                     v["user_id"] = (Json::UInt64)item.user_id;
                                     v["group_id"] = (Json::UInt64)item.group_id;
                                     v["group_name"] = item.group_name;
                                     v["remark"] = item.remark;
                                     v["avatar"] = item.avatar;
                                     v["nickname"] = item.nickname;
                                     v["created_at"] = item.created_at;
                                     list.append(v);
                                 }
                                 d["items"] = list;
                                 res->setBody(Ok(d));
                                 return 0;
                             });

        dispatch->addServlet("/api/v1/group-apply/create",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t group_id = IM::JsonUtil::GetUint64(body, "group_id");
                                 std::string remark = IM::JsonUtil::GetString(body, "remark");
                                 auto result = m_group_service->CreateApply(uid_result.data,
                                                                            group_id, remark);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });

        dispatch->addServlet("/api/v1/group-apply/decline",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t apply_id = IM::JsonUtil::GetUint64(body, "apply_id");
                                 std::string remark = IM::JsonUtil::GetString(body, "remark");
                                 auto result = m_group_service->DeclineApply(uid_result.data,
                                                                             apply_id, remark);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });

        dispatch->addServlet(
            "/api/v1/group-apply/delete",
            [this](IM::http::HttpRequest::ptr /*req*/, IM::http::HttpResponse::ptr res,
                   IM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                res->setBody(Ok());
                return 0;
            });

        dispatch->addServlet("/api/v1/group-apply/list",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t group_id = IM::JsonUtil::GetUint64(body, "group_id");
                                 auto result =
                                     m_group_service->GetApplyList(uid_result.data, group_id);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 Json::Value d;
                                 Json::Value list(Json::arrayValue);
                                 for (const auto& item : result.data) {
                                     Json::Value v;
                                     v["id"] = (Json::UInt64)item.id;
                                     v["user_id"] = (Json::UInt64)item.user_id;
                                     v["group_id"] = (Json::UInt64)item.group_id;
                                     v["remark"] = item.remark;
                                     v["avatar"] = item.avatar;
                                     v["nickname"] = item.nickname;
                                     v["created_at"] = item.created_at;
                                     list.append(v);
                                 }
                                 d["items"] = list;
                                 res->setBody(Ok(d));
                                 return 0;
                             });

        dispatch->addServlet("/api/v1/group-apply/unread-num",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 auto result =
                                     m_group_service->GetUnreadApplyCount(uid_result.data);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 Json::Value d;
                                 d["num"] = result.data;
                                 res->setBody(Ok(d));
                                 return 0;
                             });

        // 群组公告
        dispatch->addServlet("/api/v1/group-notice/edit",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t group_id = IM::JsonUtil::GetUint64(body, "group_id");
                                 std::string content = IM::JsonUtil::GetString(body, "content");
                                 auto result = m_group_service->EditNotice(uid_result.data,
                                                                           group_id, content);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });

        // 群组投票
        dispatch->addServlet("/api/v1/group-vote/create",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t group_id = IM::JsonUtil::GetUint64(body, "group_id");
                                 std::string title = IM::JsonUtil::GetString(body, "title");
                                 int answer_mode = IM::JsonUtil::GetInt32(body, "answer_mode");
                                 int is_anonymous = IM::JsonUtil::GetInt32(body, "is_anonymous");
                                 std::vector<std::string> options;
                                 if (body.isMember("options") && body["options"].isArray()) {
                                     for (const auto& opt : body["options"]) {
                                         options.push_back(opt.asString());
                                     }
                                 }
                                 auto result = m_group_service->CreateVote(
                                     uid_result.data, group_id, title, answer_mode, is_anonymous,
                                     options);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 Json::Value d;
                                 d["vote_id"] = (Json::UInt64)result.data;
                                 res->setBody(Ok(d));
                                 return 0;
                             });

        dispatch->addServlet("/api/v1/group-vote/detail",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t vote_id = IM::JsonUtil::GetUint64(body, "vote_id");
                                 auto result =
                                     m_group_service->GetVoteDetail(uid_result.data, vote_id);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 Json::Value d;
                                 d["vote_id"] = (Json::UInt64)result.data.vote_id;
                                 d["title"] = result.data.title;
                                 d["answer_mode"] = result.data.answer_mode;
                                 d["is_anonymous"] = result.data.is_anonymous;
                                 d["status"] = result.data.status;
                                 d["created_by"] = (Json::UInt64)result.data.created_by;
                                 d["created_at"] = result.data.created_at;
                                 d["voted_count"] = result.data.voted_count;
                                 d["is_voted"] = result.data.is_voted;
                                 Json::Value opts(Json::arrayValue);
                                 for (const auto& opt : result.data.options) {
                                     Json::Value o;
                                     o["id"] = (Json::UInt64)opt.id;
                                     o["content"] = opt.content;
                                     o["count"] = opt.count;
                                     o["is_voted"] = opt.is_voted;
                                     Json::Value users(Json::arrayValue);
                                     for (const auto& u : opt.users) {
                                         users.append(u);
                                     }
                                     o["users"] = users;
                                     opts.append(o);
                                 }
                                 d["options"] = opts;
                                 res->setBody(Ok(d));
                                 return 0;
                             });

        dispatch->addServlet("/api/v1/group-vote/submit",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t vote_id = IM::JsonUtil::GetUint64(body, "vote_id");
                                 std::vector<std::string> options;
                                 if (body.isMember("options") && body["options"].isArray()) {
                                     for (const auto& opt : body["options"]) {
                                         options.push_back(opt.asString());
                                     }
                                 }
                                 auto result =
                                     m_group_service->CastVote(uid_result.data, vote_id, options);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });

        dispatch->addServlet("/api/v1/group-vote/finish",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t vote_id = IM::JsonUtil::GetUint64(body, "vote_id");
                                 auto result =
                                     m_group_service->FinishVote(uid_result.data, vote_id);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });

        dispatch->addServlet("/api/v1/group-vote/list",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t group_id = IM::JsonUtil::GetUint64(body, "group_id");
                                 auto result =
                                     m_group_service->GetVoteList(uid_result.data, group_id);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 Json::Value d;
                                 Json::Value list(Json::arrayValue);
                                 for (const auto& item : result.data) {
                                     Json::Value v;
                                     v["vote_id"] = (Json::UInt64)item.vote_id;
                                     v["title"] = item.title;
                                     v["answer_mode"] = item.answer_mode;
                                     v["is_anonymous"] = item.is_anonymous;
                                     v["status"] = item.status;
                                     v["created_by"] = (Json::UInt64)item.created_by;
                                     v["created_at"] = item.created_at;
                                     v["is_voted"] = item.is_voted;
                                     list.append(v);
                                 }
                                 d["items"] = list;
                                 res->setBody(Ok(d));
                                 return 0;
                             });

        // 群组主要接口
        dispatch->addServlet("/api/v1/group/assign-admin",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t group_id = IM::JsonUtil::GetUint64(body, "group_id");
                                 uint64_t user_id = IM::JsonUtil::GetUint64(body, "user_id");
                                 int action = IM::JsonUtil::GetInt32(body, "action");
                                 auto result = m_group_service->AssignAdmin(
                                     uid_result.data, group_id, user_id, action);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });

        dispatch->addServlet("/api/v1/group/create",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 std::string name = IM::JsonUtil::GetString(body, "name");
                                 std::vector<uint64_t> member_ids;
                                 if (body.isMember("user_ids") && body["user_ids"].isArray()) {
                                     for (const auto& id : body["user_ids"]) {
                                         member_ids.push_back(id.asUInt64());
                                     }
                                 }
                                 auto result = m_group_service->CreateGroup(uid_result.data, name,
                                                                            member_ids);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 Json::Value d(Json::objectValue);
                                 d["group_id"] = (Json::UInt64)result.data;
                                 res->setBody(Ok(d));
                                 return 0;
                             });

        dispatch->addServlet("/api/v1/group/detail",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t group_id = IM::JsonUtil::GetUint64(body, "group_id");
                                 auto result =
                                     m_group_service->GetGroupDetail(uid_result.data, group_id);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 Json::Value d(Json::objectValue);
                                 d["group_id"] = (Json::UInt64)result.data.group_id;
                                 d["group_name"] = result.data.group_name;
                                 d["profile"] = result.data.profile;
                                 d["avatar"] = result.data.avatar;
                                 d["created_at"] = result.data.created_at;
                                 d["is_manager"] = result.data.is_manager;
                                 d["is_disturb"] = result.data.is_disturb;
                                 d["visit_card"] = result.data.visit_card;
                                 d["is_mute"] = result.data.is_mute;
                                 d["is_overt"] = result.data.is_overt;
                                 if (!result.data.notice.content.empty()) {
                                     Json::Value n;
                                     n["content"] = result.data.notice.content;
                                     n["created_at"] = result.data.notice.created_at;
                                     n["updated_at"] = result.data.notice.updated_at;
                                     n["modify_user_name"] = result.data.notice.modify_user_name;
                                     d["notice"] = n;
                                 }
                                 res->setBody(Ok(d));
                                 return 0;
                             });

        dispatch->addServlet("/api/v1/group/dismiss",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t group_id = IM::JsonUtil::GetUint64(body, "group_id");
                                 auto result =
                                     m_group_service->DismissGroup(uid_result.data, group_id);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });

        dispatch->addServlet(
            "/api/v1/group/get-invite-friends",
            [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                   IM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                auto uid_result = GetUidFromToken(req, res);
                if (!uid_result.ok) {
                    res->setStatus(ToHttpStatus(uid_result.code));
                    res->setBody(Error(uid_result.code, uid_result.err));
                    return 0;
                }
                Json::Value body;
                if (!ParseBody(req->getBody(), body)) {
                    res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                    return 0;
                }
                uint64_t group_id = IM::JsonUtil::GetUint64(body, "group_id");

                // 1. Get all friends
                auto friends_res = m_contact_service->ListFriends(uid_result.data);
                if (!friends_res.ok) {
                    res->setStatus(ToHttpStatus(friends_res.code));
                    res->setBody(Error(friends_res.code, friends_res.err));
                    return 0;
                }

                // 2. Get group members
                auto members_res = m_group_service->GetGroupMemberList(uid_result.data, group_id);
                if (!members_res.ok) {
                    res->setStatus(ToHttpStatus(members_res.code));
                    res->setBody(Error(members_res.code, members_res.err));
                    return 0;
                }

                // 3. Filter
                std::set<uint64_t> member_ids;
                for (const auto& m : members_res.data) {
                    member_ids.insert(m.user_id);
                }

                Json::Value d;
                Json::Value list(Json::arrayValue);
                for (const auto& f : friends_res.data) {
                    if (member_ids.find(f.user_id) == member_ids.end()) {
                        Json::Value v;
                        v["user_id"] = (Json::UInt64)f.user_id;
                        v["nickname"] = f.nickname;
                        v["avatar"] = f.avatar;
                        v["gender"] = f.gender;
                        v["motto"] = f.motto;
                        v["remark"] = f.remark;
                        list.append(v);
                    }
                }
                d["items"] = list;
                res->setBody(Ok(d));
                return 0;
            });

        dispatch->addServlet(
            "/api/v1/group/handover",
            [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                   IM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                auto uid_result = GetUidFromToken(req, res);
                if (!uid_result.ok) {
                    res->setStatus(ToHttpStatus(uid_result.code));
                    res->setBody(Error(uid_result.code, uid_result.err));
                    return 0;
                }
                Json::Value body;
                if (!ParseBody(req->getBody(), body)) {
                    res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                    return 0;
                }
                uint64_t group_id = IM::JsonUtil::GetUint64(body, "group_id");
                uint64_t user_id = IM::JsonUtil::GetUint64(body, "user_id");

                auto result = m_group_service->HandoverGroup(uid_result.data, group_id, user_id);
                if (!result.ok) {
                    res->setStatus(ToHttpStatus(result.code));
                    res->setBody(Error(result.code, result.err));
                    return 0;
                }
                res->setBody(Ok());
                return 0;
            });

        dispatch->addServlet("/api/v1/group/invite",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t group_id = IM::JsonUtil::GetUint64(body, "group_id");
                                 std::vector<uint64_t> user_ids;
                                 if (body.isMember("user_ids") && body["user_ids"].isArray()) {
                                     for (const auto& id : body["user_ids"]) {
                                         user_ids.push_back(id.asUInt64());
                                     }
                                 }
                                 auto result = m_group_service->InviteGroup(uid_result.data,
                                                                            group_id, user_ids);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });

        dispatch->addServlet("/api/v1/group/list",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 auto result = m_group_service->GetGroupList(uid_result.data);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 Json::Value d;
                                 Json::Value items(Json::arrayValue);
                                 for (const auto& item : result.data) {
                                     Json::Value v;
                                     v["group_id"] = (Json::UInt64)item.group_id;
                                     v["group_name"] = item.group_name;
                                     v["avatar"] = item.avatar;
                                     v["profile"] = item.profile;
                                     v["leader"] = (Json::UInt64)item.leader;
                                     v["creator_id"] = (Json::UInt64)item.creator_id;
                                     items.append(v);
                                 }
                                 d["items"] = items;
                                 res->setBody(Ok(d));
                                 return 0;
                             });

        dispatch->addServlet("/api/v1/group/member-list",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t group_id = IM::JsonUtil::GetUint64(body, "group_id");
                                 auto result =
                                     m_group_service->GetGroupMemberList(uid_result.data, group_id);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 Json::Value d;
                                 Json::Value list(Json::arrayValue);
                                 for (const auto& item : result.data) {
                                     Json::Value v;
                                     v["user_id"] = (Json::UInt64)item.user_id;
                                     v["nickname"] = item.nickname;
                                     v["avatar"] = item.avatar;
                                     v["gender"] = item.gender;
                                     v["leader"] = item.leader;
                                     v["is_mute"] = item.is_mute;
                                     v["remark"] = item.remark;
                                     v["motto"] = item.motto;
                                     list.append(v);
                                 }
                                 d["list"] = list;
                                 res->setBody(Ok(d));
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/group/mute", [this](IM::http::HttpRequest::ptr req,
                                                          IM::http::HttpResponse::ptr res,
                                                          IM::http::HttpSession::ptr /*session*/) {
            res->setHeader("Content-Type", "application/json");
            auto uid_result = GetUidFromToken(req, res);
            if (!uid_result.ok) {
                res->setStatus(ToHttpStatus(uid_result.code));
                res->setBody(Error(uid_result.code, uid_result.err));
                return 0;
            }
            Json::Value body;
            if (!ParseBody(req->getBody(), body)) {
                res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                return 0;
            }
            uint64_t group_id = IM::JsonUtil::GetUint64(body, "group_id");
            int action = IM::JsonUtil::GetInt32(body, "action");
            auto result = m_group_service->MuteGroup(uid_result.data, group_id, action);
            if (!result.ok) {
                res->setStatus(ToHttpStatus(result.code));
                res->setBody(Error(result.code, result.err));
                return 0;
            }
            res->setBody(Ok());
            return 0;
        });
        dispatch->addServlet("/api/v1/group/no-speak",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t group_id = IM::JsonUtil::GetUint64(body, "group_id");
                                 uint64_t user_id = IM::JsonUtil::GetUint64(body, "user_id");
                                 int action = IM::JsonUtil::GetInt32(body, "action");
                                 auto result = m_group_service->MuteMember(
                                     uid_result.data, group_id, user_id, action);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/group/overt", [this](IM::http::HttpRequest::ptr req,
                                                           IM::http::HttpResponse::ptr res,
                                                           IM::http::HttpSession::ptr /*session*/) {
            res->setHeader("Content-Type", "application/json");
            auto uid_result = GetUidFromToken(req, res);
            if (!uid_result.ok) {
                res->setStatus(ToHttpStatus(uid_result.code));
                res->setBody(Error(uid_result.code, uid_result.err));
                return 0;
            }
            Json::Value body;
            if (!ParseBody(req->getBody(), body)) {
                res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                return 0;
            }
            uint64_t group_id = IM::JsonUtil::GetUint64(body, "group_id");
            int action = IM::JsonUtil::GetInt32(body, "action");
            auto result = m_group_service->OvertGroup(uid_result.data, group_id, action);
            if (!result.ok) {
                res->setStatus(ToHttpStatus(result.code));
                res->setBody(Error(result.code, result.err));
                return 0;
            }
            res->setBody(Ok());
            return 0;
        });
        dispatch->addServlet("/api/v1/group/overt-list",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 int page = IM::JsonUtil::GetInt32(body, "page");
                                 std::string name = IM::JsonUtil::GetString(body, "name");
                                 auto result = m_group_service->GetOvertGroupList(page, name);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 Json::Value d;
                                 Json::Value list(Json::arrayValue);
                                 for (const auto& item : result.data.first) {
                                     Json::Value v;
                                     v["group_id"] = (Json::UInt64)item.group_id;
                                     v["type"] = item.type;
                                     v["name"] = item.name;
                                     v["avatar"] = item.avatar;
                                     v["profile"] = item.profile;
                                     v["count"] = item.count;
                                     v["max_num"] = item.max_num;
                                     v["is_member"] = item.is_member;
                                     v["created_at"] = item.created_at;
                                     list.append(v);
                                 }
                                 d["items"] = list;
                                 d["next"] = result.data.second;
                                 res->setBody(Ok(d));
                                 return 0;
                             });
        dispatch->addServlet(
            "/api/v1/group/remark-update",
            [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                   IM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                auto uid_result = GetUidFromToken(req, res);
                if (!uid_result.ok) {
                    res->setStatus(ToHttpStatus(uid_result.code));
                    res->setBody(Error(uid_result.code, uid_result.err));
                    return 0;
                }
                Json::Value body;
                if (!ParseBody(req->getBody(), body)) {
                    res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                    return 0;
                }
                uint64_t group_id = IM::JsonUtil::GetUint64(body, "group_id");
                std::string remark = IM::JsonUtil::GetString(body, "remark");

                auto result =
                    m_group_service->UpdateMemberRemark(uid_result.data, group_id, remark);
                if (!result.ok) {
                    res->setStatus(ToHttpStatus(result.code));
                    res->setBody(Error(result.code, result.err));
                    return 0;
                }
                res->setBody(Ok());
                return 0;
            });
        dispatch->addServlet("/api/v1/group/remove-member",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t group_id = IM::JsonUtil::GetUint64(body, "group_id");
                                 std::vector<uint64_t> user_ids;
                                 if (body.isMember("user_ids") && body["user_ids"].isArray()) {
                                     for (const auto& id : body["user_ids"]) {
                                         user_ids.push_back(id.asUInt64());
                                     }
                                 }
                                 auto result = m_group_service->RemoveMember(uid_result.data,
                                                                             group_id, user_ids);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/group/secede",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t group_id = IM::JsonUtil::GetUint64(body, "group_id");
                                 auto result =
                                     m_group_service->SecedeGroup(uid_result.data, group_id);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/group/setting",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t group_id = IM::JsonUtil::GetUint64(body, "group_id");
                                 std::string name = IM::JsonUtil::GetString(body, "group_name");
                                 std::string avatar = IM::JsonUtil::GetString(body, "avatar");
                                 std::string profile = IM::JsonUtil::GetString(body, "profile");
                                 auto result = m_group_service->UpdateGroupSetting(
                                     uid_result.data, group_id, name, avatar, profile);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });
    }
    return true;
}

}  // namespace IM::api
