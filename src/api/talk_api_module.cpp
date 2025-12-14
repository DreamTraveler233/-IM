#include "api/talk_api_module.hpp"

#include "base/macro.hpp"
#include "common/common.hpp"
#include "http/http_server.hpp"
#include "http/http_servlet.hpp"
#include "system/application.hpp"
#include "util/util.hpp"

namespace IM::api {

static auto g_logger = IM_LOG_NAME("root");

TalkApiModule::TalkApiModule(IM::domain::service::ITalkService::Ptr talk_service,
                             IM::domain::service::IUserService::Ptr user_service,
                             IM::domain::service::IMessageService::Ptr message_service)
    : Module("api.talk", "0.1.0", "builtin"),
      m_talk_service(std::move(talk_service)),
      m_user_service(std::move(user_service)),
      m_message_service(std::move(message_service)) {}

bool TalkApiModule::onServerReady() {
    std::vector<IM::TcpServer::ptr> httpServers;
    if (!IM::Application::GetInstance()->getServer("http", httpServers)) {
        IM_LOG_WARN(g_logger) << "no http servers found when registering talk routes";
        return true;
    }

    for (auto& s : httpServers) {
        auto http = std::dynamic_pointer_cast<IM::http::HttpServer>(s);
        if (!http) continue;
        auto dispatch = http->getServletDispatch();

        // 清除会话未读消息数
        dispatch->addServlet("/api/v1/talk/session-clear-unread-num",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");

                                 uint64_t to_from_id = 0;
                                 uint8_t talk_mode = 0;
                                 Json::Value body;
                                 if (ParseBody(req->getBody(), body)) {
                                     to_from_id = IM::JsonUtil::GetUint64(body, "to_from_id");
                                     talk_mode = IM::JsonUtil::GetUint8(body, "talk_mode");
                                 }

                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }

                                 // 清除未读消息数
                                 auto result = m_talk_service->clearSessionUnreadNum(
                                     uid_result.data, to_from_id, talk_mode);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }

                                 res->setBody(Ok());
                                 return 0;
                             });

        // 清空会话聊天记录(单向)
        dispatch->addServlet("/api/v1/talk/session-clear-records",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");

                                 uint64_t to_from_id = 0;
                                 uint8_t talk_mode = 0;
                                 Json::Value body;
                                 if (ParseBody(req->getBody(), body)) {
                                     to_from_id = IM::JsonUtil::GetUint64(body, "to_from_id");
                                     talk_mode = IM::JsonUtil::GetUint8(body, "talk_mode");
                                 }

                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }

                                 // 清空聊天记录
                                 auto result = m_message_service->ClearTalkRecords(
                                     uid_result.data, talk_mode, to_from_id);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }

                                 res->setBody(Ok());
                                 return 0;
                             });

        dispatch->addServlet("/api/v1/talk/session-create", [this](IM::http::HttpRequest::ptr req,
                                                                   IM::http::HttpResponse::ptr res,
                                                                   IM::http::HttpSession::ptr) {
            res->setHeader("Content-Type", "application/json");

            uint64_t to_from_id = 0;
            uint8_t talk_mode = 0;
            Json::Value body;
            if (ParseBody(req->getBody(), body)) {
                to_from_id = IM::JsonUtil::GetUint64(body, "to_from_id");
                talk_mode = IM::JsonUtil::GetUint8(body, "talk_mode");
            }

            auto uid_result = GetUidFromToken(req, res);
            if (!uid_result.ok) {
                res->setStatus(ToHttpStatus(uid_result.code));
                res->setBody(Error(uid_result.code, uid_result.err));
                return 0;
            }

            // 创建并获取会话信息
            auto result = m_talk_service->createSession(uid_result.data, to_from_id, talk_mode);
            if (!result.ok) {
                res->setStatus(ToHttpStatus(result.code));
                res->setBody(Error(result.code, result.err));
                return 0;
            }

            // 获取用户在线状态
            auto online_result = m_user_service->GetUserOnlineStatus(to_from_id);
            if (!online_result.ok) {
                res->setStatus(ToHttpStatus(online_result.code));
                res->setBody(Error(online_result.code, online_result.err));
                return 0;
            }

            Json::Value data;
            data["id"] = result.data.id;                  // 会话id
            data["talk_mode"] = result.data.talk_mode;    // 会话模式
            data["to_from_id"] = result.data.to_from_id;  // 目标用户ID
            data["is_top"] = result.data.is_top;          // 是否置顶
            data["is_disturb"] = result.data.is_disturb;  // 是否免打扰
            data["is_online"] = online_result.data;       // 目标用户在线状态
            data["is_robot"] = result.data.is_robot;      // 是否机器人
            data["name"] = result.data.name;              // 会话名称
            data["avatar"] = result.data.avatar;          // 会话头像
            data["remark"] = result.data.remark;          // 会话备注
            data["unread_num"] = result.data.unread_num;  // 未读消息数
            data["msg_text"] = result.data.msg_text;      // 最后一条消息预览文本
            data["updated_at"] = result.data.updated_at;  // 最后更新时间

            res->setBody(Ok(data));
            return 0;
        });

        dispatch->addServlet("/api/v1/talk/session-delete", [this](IM::http::HttpRequest::ptr req,
                                                                   IM::http::HttpResponse::ptr res,
                                                                   IM::http::HttpSession::ptr) {
            res->setHeader("Content-Type", "application/json");

            uint64_t to_from_id = 0;
            uint8_t talk_mode = 0;
            Json::Value body;
            if (ParseBody(req->getBody(), body)) {
                to_from_id = IM::JsonUtil::GetUint64(body, "to_from_id");
                talk_mode = IM::JsonUtil::GetUint8(body, "talk_mode");
            }

            auto uid_result = GetUidFromToken(req, res);
            if (!uid_result.ok) {
                res->setStatus(ToHttpStatus(uid_result.code));
                res->setBody(Error(uid_result.code, uid_result.err));
                return 0;
            }

            // 删除会话
            auto result = m_talk_service->deleteSession(uid_result.data, to_from_id, talk_mode);
            if (!result.ok) {
                res->setStatus(ToHttpStatus(result.code));
                res->setBody(Error(result.code, result.err));
                return 0;
            }

            res->setBody(Ok());
            return 0;
        });

        /*设置*/
        dispatch->addServlet("/api/v1/talk/session-disturb", [this](IM::http::HttpRequest::ptr req,
                                                                    IM::http::HttpResponse::ptr res,
                                                                    IM::http::HttpSession::ptr) {
            res->setHeader("Content-Type", "application/json");

            uint64_t to_from_id = 0;
            uint8_t talk_mode = 0, action = 0;
            Json::Value body;
            if (ParseBody(req->getBody(), body)) {
                to_from_id = IM::JsonUtil::GetUint64(body, "to_from_id");
                talk_mode = IM::JsonUtil::GetUint8(body, "talk_mode");
                action = IM::JsonUtil::GetUint8(body, "action");
            }

            auto uid_result = GetUidFromToken(req, res);
            if (!uid_result.ok) {
                res->setStatus(ToHttpStatus(uid_result.code));
                res->setBody(Error(uid_result.code, uid_result.err));
                return 0;
            }

            // 设置是否免打扰
            auto result =
                m_talk_service->setSessionDisturb(uid_result.data, to_from_id, talk_mode, action);
            if (!result.ok) {
                res->setStatus(ToHttpStatus(result.code));
                res->setBody(Error(result.code, result.err));
                return 0;
            }

            res->setBody(Ok());
            return 0;
        });
        dispatch->addServlet("/api/v1/talk/session-list", [this](IM::http::HttpRequest::ptr req,
                                                                 IM::http::HttpResponse::ptr res,
                                                                 IM::http::HttpSession::ptr) {
            res->setHeader("Content-Type", "application/json");

            // 获取用户ID
            auto uid_result = GetUidFromToken(req, res);
            if (!uid_result.ok) {
                res->setStatus(ToHttpStatus(uid_result.code));
                res->setBody(Error(uid_result.code, uid_result.err));
                return 0;
            }

            // 获取会话列表
            auto result = m_talk_service->getSessionListByUserId(uid_result.data);
            if (!result.ok) {
                res->setStatus(ToHttpStatus(result.code));
                res->setBody(Error(result.code, result.err));
                return 0;
            }

            Json::Value root;
            Json::Value items(Json::arrayValue);
            for (auto& item : result.data) {
                Json::Value data;
                data["id"] = item.id;                  // 会话id
                data["talk_mode"] = item.talk_mode;    // 会话模式
                data["to_from_id"] = item.to_from_id;  // 目标用户ID
                data["is_top"] = item.is_top;          // 是否置顶
                data["is_disturb"] = item.is_disturb;  // 是否免打扰
                data["is_robot"] = item.is_robot;      // 是否机器人
                data["name"] = item.name;              // 会话名称
                data["avatar"] = item.avatar;          // 会话头像
                data["remark"] = item.remark;          // 会话备注
                data["unread_num"] = item.unread_num;  // 未读消息数
                data["msg_text"] = item.msg_text;      // 最后一条消息预览文本
                data["updated_at"] = item.updated_at;  // 最后更新时间
                items.append(data);
            }
            root["items"] = items;
            res->setBody(Ok(root));
            return 0;
        });

        dispatch->addServlet("/api/v1/talk/session-top", [this](IM::http::HttpRequest::ptr req,
                                                                IM::http::HttpResponse::ptr res,
                                                                IM::http::HttpSession::ptr) {
            res->setHeader("Content-Type", "application/json");

            uint64_t to_from_id = 0;
            uint8_t talk_mode = 0, action = 0;
            Json::Value body;
            if (ParseBody(req->getBody(), body)) {
                to_from_id = IM::JsonUtil::GetUint64(body, "to_from_id");
                talk_mode = IM::JsonUtil::GetUint8(body, "talk_mode");
                action = IM::JsonUtil::GetUint8(body, "action");
            }

            auto uid_result = GetUidFromToken(req, res);
            if (!uid_result.ok) {
                res->setStatus(ToHttpStatus(uid_result.code));
                res->setBody(Error(uid_result.code, uid_result.err));
                return 0;
            }

            // 设置是否置顶
            auto result =
                m_talk_service->setSessionTop(uid_result.data, to_from_id, talk_mode, action);
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
