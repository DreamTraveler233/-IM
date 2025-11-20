#include "api/message_api_module.hpp"

#include "api/ws_gateway_module.hpp"
#include "app/message_service.hpp"
#include "base/macro.hpp"
#include "common/common.hpp"
#include "common/message_type_map.hpp"
#include "common/validate.hpp"
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
        dispatch->addServlet("/api/v1/message/delete", [](CIM::http::HttpRequest::ptr req,
                                                          CIM::http::HttpResponse::ptr res,
                                                          CIM::http::HttpSession::ptr) {
            res->setHeader("Content-Type", "application/json");

            Json::Value body;
            uint8_t talk_mode = 0;
            uint64_t to_from_id = 0;
            std::vector<std::string> msg_ids;  // 删除的消息ID列表
            if (ParseBody(req->getBody(), body)) {
                talk_mode = CIM::JsonUtil::GetUint8(body, "talk_mode");
                to_from_id = CIM::JsonUtil::GetUint64(body, "to_from_id");
                if (body.isMember("msg_ids") && body["msg_ids"].isArray()) {
                    if (!CIM::common::parseMsgIdsFromJson(body["msg_ids"], msg_ids, true)) {
                        res->setStatus(ToHttpStatus(400));
                        res->setBody(Error(400, "msg_ids 格式错误"));
                        return 0;
                    }
                }
            }

            auto uid_ret = GetUidFromToken(req, res);
            if (!uid_ret.ok) {
                res->setStatus(ToHttpStatus(uid_ret.code));
                res->setBody(Error(uid_ret.code, uid_ret.err));
                return 0;
            }

            auto svc_ret = CIM::app::MessageService::DeleteMessages(uid_ret.data, talk_mode,
                                                                    to_from_id, msg_ids);
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
                    if (!CIM::common::parseMsgIdsFromJson(body["msg_ids"], msg_ids, true)) {
                        res->setStatus(ToHttpStatus(400));
                        res->setBody(Error(400, "msg_ids 格式错误"));
                        return 0;
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
                it["sequence"] = r.sequence;
                it["msg_type"] = r.msg_type;
                it["from_id"] = r.from_id;
                it["nickname"] = r.nickname;
                it["avatar"] = r.avatar;
                it["is_revoked"] = r.is_revoked;
                it["status"] = r.status;
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
                                    it["status"] = r.status;
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
                                    it["status"] = r.status;
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
            std::vector<uint64_t> mentioned_user_ids;
            Json::Value body;
            if (ParseBody(req->getBody(), body)) {
                msg_id = CIM::JsonUtil::GetString(body, "msg_id");
                quote_id = CIM::JsonUtil::GetString(body, "quote_id");
                talk_mode = CIM::JsonUtil::GetUint8(body, "talk_mode");
                to_from_id = CIM::JsonUtil::GetUint64(body, "to_from_id");
                type = CIM::JsonUtil::GetString(body, "type");
                if (body.isMember("body")) {
                    payload = body["body"];  // 直接取 JSON
                    // 提取 mentions（数组，来自前端 editor 的 mentionUids）
                    if (payload.isMember("mentions") && payload["mentions"].isArray()) {
                        for (auto& v : payload["mentions"]) {
                            if (v.isUInt64()) {
                                mentioned_user_ids.push_back(v.asUInt64());
                            } else if (v.isString()) {
                                try {
                                    mentioned_user_ids.push_back(std::stoull(v.asString()));
                                } catch (...) {
                                    // ignore malformed id
                                }
                            }
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

            // 说明：前端传入的 `type` 是字符串（如 "text", "image", "forward" 等）
            // 服务端将其映射为内部的 `msg_type` 枚举数值（与数据库中 msg_type 字段一致），
            // 在 `MessageService::SendMessage` 中会用到这个数值分支实现不同类型的保存策略。
            uint16_t msg_type = 0;
            static const auto& kTypeMap = CIM::common::kMessageTypeMap;
            auto it = kTypeMap.find(type);
            if (it != kTypeMap.end()) {
                msg_type = static_cast<uint16_t>(it->second);
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

            std::string content_text;  // 文本类消息正文
            std::string extra;         // 非文本消息/扩展字段 JSON 字符串
            if (msg_type == static_cast<uint16_t>(CIM::common::MessageType::Text)) {
                if (payload.isMember("text")) {
                    content_text = CIM::JsonUtil::GetString(payload, "text");
                }

                // 服务器端也禁止发送空白消息
                if (CIM::StringUtil::Trim(content_text, " \t\n\r").empty()) {
                    res->setStatus(ToHttpStatus(400));
                    res->setBody(Error(400, "消息内容不能为空"));
                    return 0;
                }
            } else {
                Json::StreamWriterBuilder wb;
                extra = Json::writeString(wb, payload);
            }

            // 如果是转发并且包含目标 user_ids/group_ids，则对每个目标分发消息
            Json::Value dist_root;
            if (msg_type == static_cast<uint16_t>(CIM::common::MessageType::Forward) &&
                ((payload.isMember("user_ids") && payload["user_ids"].isArray()) ||
                 (payload.isMember("group_ids") && payload["group_ids"].isArray()))) {
                // 解析 action / msg_ids / targets
                int action = CIM::JsonUtil::GetInt32(payload, "action");
                std::vector<std::string> forward_msg_ids;
                if (payload.isMember("msg_ids") && payload["msg_ids"].isArray()) {
                    if (!CIM::common::parseMsgIdsFromJson(payload["msg_ids"], forward_msg_ids,
                                                          true)) {
                        res->setStatus(ToHttpStatus(400));
                        res->setBody(Error(400, "msg_ids 格式错误"));
                        return 0;
                    }
                }

                // 分发函数
                // 说明：target_mode: 1=用户，2=群组；target_id: 具体 ID，forward_payload: 转发消息负载
                bool sender_reload_notified = false;

                auto send_to_target = [&](uint8_t target_mode, uint64_t target_id,
                                          const Json::Value& forward_payload) {
                    Json::StreamWriterBuilder wb;
                    std::string fextra = Json::writeString(wb, forward_payload);
                    // 不传 msg_id 保证每个目标生成独立 ID（也可以根据需求由前端传入）
                    auto r = CIM::app::MessageService::SendMessage(
                        uid_ret.data, target_mode, target_id, msg_type, std::string(), fextra,
                        quote_id, std::string(), std::vector<uint64_t>());
                    Json::Value item;
                    if (!r.ok) {
                        item["ok"] = false;
                        item["err"] = r.err;
                    } else {
                        item["ok"] = true;
                        item["msg_id"] = r.data.msg_id;
                        item["to_talk_mode"] = target_mode;
                        item["to_id"] = (Json::UInt64)target_id;
                    }
                    dist_root.append(item);

                    // 仅记录发送者是否需要刷新会话（推送延后合并），避免重复触发多次 reload
                    if (r.ok) {
                        sender_reload_notified = true;
                    }
                };

                // if action==1 (single), send each msg_id separately
                if (action == 1) {
                    // 对 user_ids
                    if (payload.isMember("user_ids") && payload["user_ids"].isArray()) {
                        for (auto& v : payload["user_ids"]) {
                            uint64_t uid_target = 0;
                            if (v.isUInt64())
                                uid_target = v.asUInt64();
                            else if (v.isString())
                                uid_target = std::stoull(v.asString());
                            for (auto& mid : forward_msg_ids) {
                                Json::Value fp(Json::objectValue);
                                Json::Value arr(Json::arrayValue);
                                arr.append(mid);
                                fp["msg_ids"] = arr;
                                send_to_target(1, uid_target, fp);
                            }
                        }
                    }
                    // 对 group_ids
                    if (payload.isMember("group_ids") && payload["group_ids"].isArray()) {
                        for (auto& v : payload["group_ids"]) {
                            uint64_t gid_target = 0;
                            if (v.isUInt64())
                                gid_target = v.asUInt64();
                            else if (v.isString())
                                gid_target = std::stoull(v.asString());
                            for (auto& mid : forward_msg_ids) {
                                Json::Value fp(Json::objectValue);
                                Json::Value arr(Json::arrayValue);
                                arr.append(mid);
                                fp["msg_ids"] = arr;
                                send_to_target(2, gid_target, fp);
                            }
                        }
                    }
                } else {
                    // action==2: 合并转发 -> send a single message with msg_ids[] for each target
                    Json::Value fp(Json::objectValue);
                    Json::Value arr(Json::arrayValue);
                    for (auto& mid : forward_msg_ids) arr.append(mid);
                    fp["msg_ids"] = arr;

                    if (payload.isMember("user_ids") && payload["user_ids"].isArray()) {
                        for (auto& v : payload["user_ids"]) {
                            uint64_t uid_target = 0;
                            if (v.isUInt64())
                                uid_target = v.asUInt64();
                            else if (v.isString())
                                uid_target = std::stoull(v.asString());
                            send_to_target(1, uid_target, fp);
                        }
                    }
                    if (payload.isMember("group_ids") && payload["group_ids"].isArray()) {
                        for (auto& v : payload["group_ids"]) {
                            uint64_t gid_target = 0;
                            if (v.isUInt64())
                                gid_target = v.asUInt64();
                            else if (v.isString())
                                gid_target = std::stoull(v.asString());
                            send_to_target(2, gid_target, fp);
                        }
                    }
                }

                // 合并推送：若本次转发有至少一个成功的发送，则触发一次会话刷新通知给发送者
                if (sender_reload_notified) {
                    CIM::api::WsGatewayModule::PushToUser(uid_ret.data, "im.session.reload");
                }

                // 返回分发结果给前端，前端目前不依赖具体返回值，仅关心是否成功
                Json::Value root;
                root["items"] = dist_root;
                res->setBody(Ok(root));
                return 0;
            }
            // 非转发分发（或没有指定 user_ids/group_ids）则把消息作为普通消息发送
            auto svc_ret = CIM::app::MessageService::SendMessage(
                uid_ret.data, talk_mode, to_from_id, msg_type, content_text, extra, quote_id,
                msg_id, mentioned_user_ids);
            if (!svc_ret.ok) {
                res->setStatus(ToHttpStatus(svc_ret.code));
                res->setBody(Error(svc_ret.code, svc_ret.err));
                return 0;
            }

            // 构造响应（REST 返回）
            // 这里返回的 JSON 结构和 WebSocket 推送保持一致，方便前端统一取用：
            //  - 前端在发送成功后可以直接用 REST 返回渲染出本端消息
            //  - WebSocket 推送用于通知对端/其它设备显示该消息
            Json::Value root;
            const auto& r = svc_ret.data;
            root["msg_id"] = r.msg_id;
            root["sequence"] = (Json::UInt64)r.sequence;
            root["msg_type"] = r.msg_type;
            root["from_id"] = (Json::UInt64)r.from_id;
            root["nickname"] = r.nickname;
            root["avatar"] = r.avatar;
            root["is_revoked"] = r.is_revoked;
            root["status"] = r.status;
            root["send_time"] = r.send_time;
            root["extra"] = r.extra;
            root["quote"] = r.quote;
            res->setBody(Ok(root));
            return 0;
        });

        // 更新消息状态（sender 更新发送状态，如标记失败/成功）
        dispatch->addServlet("/api/v1/message/status",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 Json::Value body;
                                 uint8_t talk_mode = 0;
                                 uint64_t to_from_id = 0;
                                 std::string msg_id;
                                 uint8_t status = 1;
                                 if (ParseBody(req->getBody(), body)) {
                                     talk_mode = CIM::JsonUtil::GetUint8(body, "talk_mode");
                                     to_from_id = CIM::JsonUtil::GetUint64(body, "to_from_id");
                                     msg_id = CIM::JsonUtil::GetString(body, "msg_id");
                                     status = CIM::JsonUtil::GetUint8(body, "status");
                                 }
                                 auto uid_ret = GetUidFromToken(req, res);
                                 if (!uid_ret.ok) {
                                     res->setStatus(ToHttpStatus(uid_ret.code));
                                     res->setBody(Error(uid_ret.code, uid_ret.err));
                                     return 0;
                                 }
                                 auto svc_ret = CIM::app::MessageService::UpdateMessageStatus(
                                     uid_ret.data, talk_mode, to_from_id, msg_id, status);
                                 if (!svc_ret.ok) {
                                     res->setStatus(ToHttpStatus(svc_ret.code));
                                     res->setBody(Error(svc_ret.code, svc_ret.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });
    }
    return true;
}

}  // namespace CIM::api
