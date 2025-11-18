#include "api/message_api_module.hpp"

#include "api/ws_gateway_module.hpp"
#include "app/message_service.hpp"
#include "base/macro.hpp"
#include "common/common.hpp"
#include "http/http_server.hpp"
#include "http/http_servlet.hpp"
#include "system/application.hpp"
#include "util/util.hpp"

namespace CIM::api {

static auto g_logger = CIM_LOG_NAME("root");

MessageApiModule::MessageApiModule() : Module("api.message", "0.1.0", "builtin") {}

bool MessageApiModule::onServerReady() {
    std::vector<CIM::TcpServer::ptr> httpServers;
    if (!CIM::Application::GetInstance()->getServer("http", httpServers)) {
        CIM_LOG_WARN(g_logger) << "no http servers found when registering message routes";
        return true;
    }

    for (auto& s : httpServers) {
        auto http = std::dynamic_pointer_cast<CIM::http::HttpServer>(s);
        if (!http) continue;
        auto dispatch = http->getServletDispatch();

        // 删除消息（仅影响本人视图）
        dispatch->addServlet("/api/v1/message/delete",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 Json::Value body;
                                 uint8_t talk_mode = 0;
                                 uint64_t to_from_id = 0;
                                 std::vector<std::string> msg_ids;
                                 if (ParseBody(req->getBody(), body)) {
                                     talk_mode = CIM::JsonUtil::GetUint8(body, "talk_mode");
                                     to_from_id = CIM::JsonUtil::GetUint64(body, "to_from_id");
                                     if (body.isMember("msg_ids") && body["msg_ids"].isArray()) {
                                         for (auto& v : body["msg_ids"]) {
                                             if (v.isString()) {
                                                 msg_ids.push_back(v.asString());
                                             } else if (v.isUInt64()) {
                                                 msg_ids.push_back(std::to_string(v.asUInt64()));
                                             }
                                         }
                                     }
                                 }
                                 auto uid_ret = GetUidFromToken(req, res);
                                 if (!uid_ret.ok) {
                                     res->setStatus(ToHttpStatus(uid_ret.code));
                                     res->setBody(Error(uid_ret.code, uid_ret.err));
                                     return 0;
                                 }
                                 auto svc_ret = CIM::app::MessageService::DeleteMessages(
                                     uid_ret.data, talk_mode, to_from_id, msg_ids);
                                 if (!svc_ret.ok) {
                                     res->setStatus(ToHttpStatus(svc_ret.code));
                                     res->setBody(Error(svc_ret.code, svc_ret.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });

        // 转发消息记录查询（不分页）
        dispatch->addServlet("/api/v1/message/forward-records", [](CIM::http::HttpRequest::ptr req,
                                                                   CIM::http::HttpResponse::ptr res,
                                                                   CIM::http::HttpSession::ptr) {
            res->setHeader("Content-Type", "application/json");
            Json::Value body;
            uint8_t talk_mode = 0;
            std::vector<std::string> msg_ids;
            if (ParseBody(req->getBody(), body)) {
                talk_mode = CIM::JsonUtil::GetUint8(body, "talk_mode");
                if (body.isMember("msg_ids") && body["msg_ids"].isArray()) {
                    for (auto& v : body["msg_ids"]) {
                        if (v.isString()) {
                            msg_ids.push_back(v.asString());
                        } else if (v.isUInt64()) {
                            msg_ids.push_back(std::to_string(v.asUInt64()));
                        }
                    }
                }
            }
            auto uid_ret = GetUidFromToken(req, res);
            if (!uid_ret.ok) {
                res->setStatus(ToHttpStatus(uid_ret.code));
                res->setBody(Error(uid_ret.code, uid_ret.err));
                return 0;
            }
            auto svc_ret =
                CIM::app::MessageService::LoadForwardRecords(uid_ret.data, talk_mode, msg_ids);
            if (!svc_ret.ok) {
                res->setStatus(ToHttpStatus(svc_ret.code));
                res->setBody(Error(svc_ret.code, svc_ret.err));
                return 0;
            }
            Json::Value root;
            Json::Value items(Json::arrayValue);
            for (auto& r : svc_ret.data) {
                Json::Value it;
                it["msg_id"] = r.msg_id;
                it["sequence"] = (Json::UInt64)r.sequence;
                it["msg_type"] = r.msg_type;
                it["from_id"] = (Json::UInt64)r.from_id;
                it["nickname"] = r.nickname;
                it["avatar"] = r.avatar;
                it["is_revoked"] = r.is_revoked;
                it["send_time"] = r.send_time;
                it["extra"] = r.extra;
                it["quote"] = r.quote;
                items.append(it);
            }
            root["items"] = items;
            res->setBody(Ok(root));
            return 0;
        });

        // 历史消息分页（按类型过滤）
        dispatch->addServlet("/api/v1/message/history-records",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 Json::Value body;
                                 uint8_t talk_mode = 0;
                                 uint64_t to_from_id = 0;
                                 uint64_t cursor = 0;
                                 uint32_t limit = 0;
                                 uint16_t msg_type = 0;
                                 if (ParseBody(req->getBody(), body)) {
                                     talk_mode = CIM::JsonUtil::GetUint8(body, "talk_mode");
                                     to_from_id = CIM::JsonUtil::GetUint64(body, "to_from_id");
                                     cursor = CIM::JsonUtil::GetUint64(body, "cursor");
                                     limit = CIM::JsonUtil::GetUint32(body, "limit");
                                     msg_type = CIM::JsonUtil::GetUint16(body, "msg_type");
                                 }
                                 auto uid_ret = GetUidFromToken(req, res);
                                 if (!uid_ret.ok) {
                                     res->setStatus(ToHttpStatus(uid_ret.code));
                                     res->setBody(Error(uid_ret.code, uid_ret.err));
                                     return 0;
                                 }
                                 auto svc_ret = CIM::app::MessageService::LoadHistoryRecords(
                                     uid_ret.data, talk_mode, to_from_id, msg_type, cursor, limit);
                                 if (!svc_ret.ok) {
                                     res->setStatus(ToHttpStatus(svc_ret.code));
                                     res->setBody(Error(svc_ret.code, svc_ret.err));
                                     return 0;
                                 }
                                 Json::Value root;
                                 Json::Value items(Json::arrayValue);
                                 for (auto& r : svc_ret.data.items) {
                                     Json::Value it;
                                     it["msg_id"] = r.msg_id;
                                     it["sequence"] = (Json::UInt64)r.sequence;
                                     it["msg_type"] = r.msg_type;
                                     it["from_id"] = (Json::UInt64)r.from_id;
                                     it["nickname"] = r.nickname;
                                     it["avatar"] = r.avatar;
                                     it["is_revoked"] = r.is_revoked;
                                     it["send_time"] = r.send_time;
                                     it["extra"] = r.extra;
                                     it["quote"] = r.quote;
                                     items.append(it);
                                 }
                                 root["items"] = items;
                                 root["cursor"] = (Json::UInt64)svc_ret.data.cursor;
                                 res->setBody(Ok(root));
                                 return 0;
                             });

        /*获取会话消息记录*/
        dispatch->addServlet("/api/v1/message/records",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");

                                 Json::Value body;
                                 uint8_t talk_mode = 0;    // 会话类型
                                 uint64_t to_from_id = 0;  // 会话对象ID
                                 uint64_t cursor = 0;      // 游标
                                 uint32_t limit = 0;       // 每次请求返回的消息数量上限
                                 if (ParseBody(req->getBody(), body)) {
                                     talk_mode = CIM::JsonUtil::GetUint8(body, "talk_mode");
                                     to_from_id = CIM::JsonUtil::GetUint64(body, "to_from_id");
                                     cursor = CIM::JsonUtil::GetUint64(body, "cursor");
                                     limit = CIM::JsonUtil::GetUint32(body, "limit");
                                 }

                                 auto uid_ret = GetUidFromToken(req, res);
                                 if (!uid_ret.ok) {
                                     res->setStatus(ToHttpStatus(uid_ret.code));
                                     res->setBody(Error(uid_ret.code, uid_ret.err));
                                     return 0;
                                 }

                                 auto svc_ret = CIM::app::MessageService::LoadRecords(
                                     uid_ret.data, talk_mode, to_from_id, cursor, limit);
                                 if (!svc_ret.ok) {
                                     res->setStatus(ToHttpStatus(svc_ret.code));
                                     res->setBody(Error(svc_ret.code, svc_ret.err));
                                     return 0;
                                 }

                                 Json::Value root;
                                 Json::Value items(Json::arrayValue);
                                 for (auto& r : svc_ret.data.items) {
                                     Json::Value it;
                                     it["msg_id"] = r.msg_id;
                                     it["sequence"] = r.sequence;
                                     it["msg_type"] = r.msg_type;
                                     it["from_id"] = r.from_id;
                                     it["nickname"] = r.nickname;
                                     it["avatar"] = r.avatar;
                                     it["is_revoked"] = r.is_revoked;
                                     it["send_time"] = r.send_time;
                                     it["extra"] = r.extra;
                                     it["quote"] = r.quote;
                                     items.append(it);
                                 }
                                 root["items"] = items;
                                 root["cursor"] = svc_ret.data.cursor;
                                 res->setBody(Ok(root));
                                 return 0;
                             });

        /*消息撤回接口*/
        dispatch->addServlet("/api/v1/message/revoke",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");

                                 Json::Value body;
                                 uint8_t talk_mode = 0;               // 会话类型
                                 uint64_t to_from_id = 0;             // 会话对象ID
                                 std::string msg_id = std::string();  // 消息ID（字符串）
                                 if (ParseBody(req->getBody(), body)) {
                                     talk_mode = CIM::JsonUtil::GetUint8(body, "talk_mode");
                                     to_from_id = CIM::JsonUtil::GetUint64(body, "to_from_id");
                                     msg_id = CIM::JsonUtil::GetString(body, "msg_id");
                                 }

                                 auto uid_ret = GetUidFromToken(req, res);
                                 if (!uid_ret.ok) {
                                     res->setStatus(ToHttpStatus(uid_ret.code));
                                     res->setBody(Error(uid_ret.code, uid_ret.err));
                                     return 0;
                                 }

                                 auto svc_ret = CIM::app::MessageService::RevokeMessage(
                                     uid_ret.data, talk_mode, to_from_id, msg_id);
                                 if (!svc_ret.ok) {
                                     res->setStatus(ToHttpStatus(svc_ret.code));
                                     res->setBody(Error(svc_ret.code, svc_ret.err));
                                     return 0;
                                 }

                                 res->setBody(Ok());
                                 return 0;
                             });

        // 发送消息接口
        dispatch->addServlet("/api/v1/message/send", [](CIM::http::HttpRequest::ptr req,
                                                        CIM::http::HttpResponse::ptr res,
                                                        CIM::http::HttpSession::ptr) {
            res->setHeader("Content-Type", "application/json");

            std::string msg_id;       // 前端生成的消息ID（字符串）
            std::string quote_id;     // 引用消息ID（字符串）
            uint8_t talk_mode = 0;    // 会话类型
            uint64_t to_from_id = 0;  // 单聊对端用户ID / 群ID
            std::string type;         // 前端传入的消息类型字符串
            Json::Value payload;      // body 内容
            Json::Value body;
            if (ParseBody(req->getBody(), body)) {
                msg_id = CIM::JsonUtil::GetString(body, "msg_id");
                quote_id = CIM::JsonUtil::GetString(body, "quote_id");
                talk_mode = CIM::JsonUtil::GetUint8(body, "talk_mode");
                to_from_id = CIM::JsonUtil::GetUint64(body, "to_from_id");
                type = CIM::JsonUtil::GetString(body, "type");
                if (body.isMember("body")) {
                    payload = body["body"];  // 直接取 JSON
                }
            }

            auto uid_ret = GetUidFromToken(req, res);
            if (!uid_ret.ok) {
                res->setStatus(ToHttpStatus(uid_ret.code));
                res->setBody(Error(uid_ret.code, uid_ret.err));
                return 0;
            }

            // 类型映射（简单，后续可抽出常量）
            uint16_t msg_type = 0;
            static const std::map<std::string, uint16_t> kTypeMap = {
                {"text", 1},  {"code", 2},     {"image", 3},        {"audio", 4},   {"video", 5},
                {"file", 6},  {"location", 7}, {"card", 8},         {"forward", 9}, {"login", 10},
                {"vote", 11}, {"mixed", 12},   {"group_notice", 13}};
            auto it = kTypeMap.find(type);
            if (it != kTypeMap.end()) {
                msg_type = it->second;
            } else {
                res->setStatus(ToHttpStatus(400));
                res->setBody(Error(400, "未知消息类型"));
                return 0;
            }

            // 基础校验：msg_id 必须为 32位hex（可按需放宽）
            auto isHex32 = [](const std::string& s) {
                if (s.size() != 32) return false;
                for (char c : s) {
                    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                          (c >= 'A' && c <= 'F')))
                        return false;
                }
                return true;
            };
            if (!msg_id.empty() && !isHex32(msg_id)) {
                res->setStatus(ToHttpStatus(400));
                res->setBody(Error(400, "msg_id 必须为32位HEX字符串"));
                return 0;
            }

            std::string content_text;
            std::string extra;
            if (msg_type == 1) {  // 文本消息
                if (payload.isMember("text")) {
                    content_text = CIM::JsonUtil::GetString(payload, "text");
                }
            } else {
                Json::StreamWriterBuilder wb;
                extra = Json::writeString(wb, payload);
            }

            auto svc_ret =
                CIM::app::MessageService::SendMessage(uid_ret.data, talk_mode, to_from_id, msg_type,
                                                      content_text, extra, quote_id, msg_id);
            if (!svc_ret.ok) {
                res->setStatus(ToHttpStatus(svc_ret.code));
                res->setBody(Error(svc_ret.code, svc_ret.err));
                return 0;
            }

            // 构造响应
            Json::Value root;
            const auto& r = svc_ret.data;
            root["msg_id"] = r.msg_id;
            root["sequence"] = (Json::UInt64)r.sequence;
            root["msg_type"] = r.msg_type;
            root["from_id"] = (Json::UInt64)r.from_id;
            root["nickname"] = r.nickname;
            root["avatar"] = r.avatar;
            root["is_revoked"] = r.is_revoked;
            root["send_time"] = r.send_time;
            root["extra"] = r.extra;
            root["quote"] = r.quote;
            res->setBody(Ok(root));

            // 主动推送给对端（以及发送者其它设备），前端监听事件: im.message
            // 复用与 REST 返回一致的 body 结构
            Json::Value body_json;
            body_json["msg_id"] = r.msg_id;
            body_json["sequence"] = (Json::UInt64)r.sequence;
            body_json["msg_type"] = r.msg_type;
            body_json["from_id"] = (Json::UInt64)r.from_id;
            body_json["nickname"] = r.nickname;
            body_json["avatar"] = r.avatar;
            body_json["is_revoked"] = r.is_revoked;
            body_json["send_time"] = r.send_time;
            body_json["extra"] = r.extra;
            body_json["quote"] = r.quote;

            CIM::api::WsGatewayModule::PushImMessage(talk_mode, to_from_id, r.from_id, body_json);
            return 0;
        });
    }
    return true;
}

}  // namespace CIM::api
