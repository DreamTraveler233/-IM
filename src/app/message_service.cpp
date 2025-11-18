#include "app/message_service.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "api/ws_gateway_module.hpp"
#include "base/macro.hpp"
#include "dao/message_dao.hpp"
#include "dao/message_forward_map_dao.hpp"
#include "dao/message_mention_dao.hpp"
#include "dao/message_read_dao.hpp"
#include "dao/message_user_delete_dao.hpp"
#include "dao/talk_dao.hpp"
#include "dao/talk_sequence_dao.hpp"
#include "dao/talk_session_dao.hpp"
#include "dao/user_dao.hpp"
#include "util/hash_util.hpp"

namespace CIM::app {

static auto g_logger = CIM_LOG_NAME("root");
static constexpr const char* kDBName = "default";

uint64_t MessageService::resolveTalkId(const uint8_t talk_mode, const uint64_t to_from_id) {
    std::string err;
    uint64_t talk_id = 0;

    if (talk_mode == 1) {
        // 单聊: to_from_id 是对端用户ID，需要与当前用户排序，这里无法确定 current_user_id -> 由调用方预先取得 talk_id 更合理。
        // 为简化：此方法仅用于群聊分支；单聊应外层自行处理。返回 0 表示未解析。
        return 0;
    } else if (talk_mode == 2) {
        if (!CIM::dao::TalkDao::getGroupTalkId(to_from_id, talk_id, &err)) {
            // 不存在直接返回 0
            return 0;
        }
        return talk_id;
    }
    return 0;
}

bool MessageService::buildRecord(const CIM::dao::Message& msg, CIM::dao::MessageRecord& out,
                                 std::string* err) {
    out.msg_id = msg.id;
    out.sequence = msg.sequence;
    out.msg_type = msg.msg_type;
    out.from_id = msg.sender_id;
    out.is_revoked = msg.is_revoked;
    out.send_time = TimeUtil::TimeToStr(msg.created_at);
    out.extra = msg.extra;  // 原样透传 JSON 字符串
    out.quote = "{}";

    // 对于文本消息，前端渲染依赖 extra.content，这里补齐
    if (msg.msg_type == 1) {
        Json::Value extra;
        extra["content"] = msg.content_text;
        Json::StreamWriterBuilder wb;
        out.extra = Json::writeString(wb, extra);
    }

    // 加载用户信息（昵称/头像）
    CIM::dao::UserInfo ui;
    if (!CIM::dao::UserDAO::GetUserInfoSimple(msg.sender_id, ui, err)) {
        // 若加载失败仍返回基础字段
        out.nickname = "";
        out.avatar = "";
    } else {
        out.nickname = ui.nickname;
        out.avatar = ui.avatar;
    }

    // 引用消息
    if (!msg.quote_msg_id.empty()) {
        CIM::dao::Message quoted;
        std::string qerr;
        if (CIM::dao::MessageDao::GetById(msg.quote_msg_id, quoted, &qerr)) {
            // 适配前端结构：{"quote_id":"...","content":"...","from_id":...}
            Json::Value qjson;
            qjson["quote_id"] = quoted.id;
            qjson["from_id"] = (Json::UInt64)quoted.sender_id;
            qjson["content"] = quoted.content_text;  // 仅文本简化
            Json::StreamWriterBuilder wbq;
            out.quote = Json::writeString(wbq, qjson);
        }
    }
    return true;
}

MessageRecordPageResult MessageService::LoadRecords(const uint64_t current_user_id,
                                                    const uint8_t talk_mode,
                                                    const uint64_t to_from_id, uint64_t cursor,
                                                    uint32_t limit) {
    MessageRecordPageResult result;
    std::string err;
    if (limit == 0) {
        limit = 30;
    } else if (limit > 200) {
        limit = 200;
    }

    // 解析 talk_id
    uint64_t talk_id = 0;
    if (talk_mode == 1) {
        // 单聊，需要根据两个用户ID排序
        if (!CIM::dao::TalkDao::getSingleTalkId(current_user_id, to_from_id, talk_id, &err)) {
            result.ok = true;  // 无历史记录
            return result;
        }
    } else if (talk_mode == 2) {
        if (!CIM::dao::TalkDao::getGroupTalkId(to_from_id, talk_id, &err)) {
            result.ok = true;  // 群尚未产生消息
            return result;
        }
    } else {
        result.code = 400;
        result.err = "非法会话类型";
        return result;
    }

    std::vector<CIM::dao::Message> msgs;
    // 使用带过滤的查询，过滤掉已被当前用户删除的消息（im_message_user_delete）
    if (!CIM::dao::MessageDao::ListRecentDescWithFilter(talk_id, cursor, limit,
                                                        /*user_id=*/current_user_id,
                                                        /*msg_type=*/0, msgs, &err)) {
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger)
                << "LoadRecords ListRecentDescWithFilter failed, talk_id=" << talk_id
                << ", err=" << err;
            result.code = 500;
            result.err = "加载消息失败";
            return result;
        }
    }

    CIM::dao::MessagePage page;
    for (auto& m : msgs) {
        CIM::dao::MessageRecord rec;
        std::string rerr;
        buildRecord(m, rec, &rerr);
        page.items.push_back(std::move(rec));
    }
    if (!page.items.empty()) {
        // 下一游标为当前页最小 sequence
        uint64_t min_seq = page.items.back().sequence;
        page.cursor = min_seq;
    } else {
        page.cursor = cursor;  // 保持不变
    }
    result.data = std::move(page);
    result.ok = true;
    return result;
}

MessageRecordPageResult MessageService::LoadHistoryRecords(const uint64_t current_user_id,
                                                           const uint8_t talk_mode,
                                                           const uint64_t to_from_id,
                                                           const uint16_t msg_type, uint64_t cursor,
                                                           uint32_t limit) {
    MessageRecordPageResult result;
    std::string err;
    if (limit == 0)
        limit = 30;
    else if (limit > 200)
        limit = 200;

    uint64_t talk_id = 0;
    if (talk_mode == 1) {
        if (!CIM::dao::TalkDao::getSingleTalkId(current_user_id, to_from_id, talk_id, &err)) {
            result.ok = true;
            return result;
        }
    } else if (talk_mode == 2) {
        if (!CIM::dao::TalkDao::getGroupTalkId(to_from_id, talk_id, &err)) {
            result.ok = true;
            return result;
        }
    } else {
        result.code = 400;
        result.err = "非法会话类型";
        return result;
    }

    // 先取一页，再过滤类型（简单实现；可优化为 SQL 条件）
    std::vector<CIM::dao::Message> msgs;
    if (!CIM::dao::MessageDao::ListRecentDesc(talk_id, cursor, limit * 3, msgs,
                                              &err)) {  // 加大抓取保证过滤后足够
        result.code = 500;
        result.err = "加载消息失败";
        return result;
    }

    CIM::dao::MessagePage page;
    for (auto& m : msgs) {
        if (msg_type != 0 && m.msg_type != msg_type) continue;
        CIM::dao::MessageRecord rec;
        std::string rerr;
        buildRecord(m, rec, &rerr);
        page.items.push_back(std::move(rec));
        if (page.items.size() >= limit) break;
    }
    if (!page.items.empty()) {
        page.cursor = page.items.back().sequence;
    } else {
        page.cursor = cursor;
    }
    result.data = std::move(page);
    result.ok = true;
    return result;
}

MessageRecordListResult MessageService::LoadForwardRecords(
    const uint64_t current_user_id, const uint8_t talk_mode,
    const std::vector<std::string>& msg_ids) {
    MessageRecordListResult result;
    std::string err;
    if (msg_ids.empty()) {
        result.ok = true;
        return result;
    }

    // 简化：直接批量拉取这些消息
    for (auto& mid : msg_ids) {
        CIM::dao::Message m;
        std::string merr;
        if (!CIM::dao::MessageDao::GetById(mid, m, &merr)) continue;  // 忽略不存在
        CIM::dao::MessageRecord rec;
        std::string rerr;
        buildRecord(m, rec, &rerr);
        result.data.push_back(std::move(rec));
    }
    result.ok = true;
    return result;
}

VoidResult MessageService::DeleteMessages(const uint64_t current_user_id, const uint8_t talk_mode,
                                          const uint64_t to_from_id,
                                          const std::vector<std::string>& msg_ids) {
    VoidResult result;
    std::string err;
    if (msg_ids.empty()) {
        result.ok = true;
        return result;
    }
    auto db = CIM::MySQLMgr::GetInstance()->get("default");
    if (!db) {
        result.code = 500;
        result.err = "数据库连接失败";
        return result;
    }

    // 验证会话存在（不严格校验每条消息归属以减少查询；生产可增强）
    uint64_t talk_id = 0;
    std::string terr;
    if (talk_mode == 1) {
        if (!CIM::dao::TalkDao::getSingleTalkId(current_user_id, to_from_id, talk_id, &terr)) {
            result.ok = true;
            return result;
        }
    } else if (talk_mode == 2) {
        if (!CIM::dao::TalkDao::getGroupTalkId(to_from_id, talk_id, &terr)) {
            result.ok = true;
            return result;
        }
    } else {
        result.code = 400;
        result.err = "非法会话类型";
        return result;
    }

    for (auto& mid : msg_ids) {
        if (!CIM::dao::MessageUserDeleteDao::MarkUserDelete(mid, current_user_id, &err)) {
            CIM_LOG_WARN(g_logger)
                << "DeleteMessages MarkUserDelete failed msg_id=" << mid << " err=" << err;
        }
    }
    // 标记删除后，需要更新会话的最后消息摘要（仅影响当前用户的会话视图）
    std::vector<CIM::dao::Message> remain_msgs;
    if (!CIM::dao::MessageDao::ListRecentDescWithFilter(talk_id, /*anchor_seq=*/0,
                                                        /*limit=*/1, current_user_id,
                                                        /*msg_type=*/0, remain_msgs, &err)) {
        CIM_LOG_WARN(g_logger) << "ListRecentDescWithFilter failed: " << err;
    } else {
        if (!remain_msgs.empty()) {
            const auto& lm = remain_msgs[0];
            std::string digest;
            switch (lm.msg_type) {
                case 1:
                    digest = lm.content_text;
                    if (digest.size() > 255) digest = digest.substr(0, 255);
                    break;
                case 3:
                    digest = "[图片消息]";
                    break;
                case 4:
                    digest = "[语音消息]";
                    break;
                case 5:
                    digest = "[视频消息]";
                    break;
                case 6:
                    digest = "[文件消息]";
                    break;
                default:
                    digest = "[非文本消息]";
                    break;
            }

            if (!CIM::dao::TalkSessionDAO::updateLastMsgForUser(
                    current_user_id, talk_id, std::optional<std::string>(lm.id),
                    std::optional<uint16_t>(lm.msg_type), std::optional<uint64_t>(lm.sender_id),
                    std::optional<std::string>(digest), &err)) {
                CIM_LOG_WARN(g_logger) << "updateLastMsgForUser failed: " << err;
            }
            // 通知客户端更新会话预览
            {
                Json::Value payload;
                payload["talk_mode"] = talk_mode;
                payload["to_from_id"] = to_from_id;
                payload["msg_text"] = digest;
                payload["updated_at"] = (Json::UInt64)CIM::TimeUtil::NowToMS();
                CIM::api::WsGatewayModule::PushToUser(current_user_id, "im.session.update",
                                                      payload);
            }
        } else {
            // 没有剩余消息，清空最后消息字段
            if (!CIM::dao::TalkSessionDAO::updateLastMsgForUser(
                    current_user_id, talk_id, std::optional<std::string>(),
                    std::optional<uint16_t>(), std::optional<uint64_t>(),
                    std::optional<std::string>(), &err)) {
                CIM_LOG_WARN(g_logger) << "clear last msg for user failed: " << err;
            }
            // 通知客户端清空会话预览
            {
                Json::Value payload;
                payload["talk_mode"] = talk_mode;
                payload["to_from_id"] = to_from_id;
                payload["msg_text"] = Json::Value();
                payload["updated_at"] = (Json::UInt64)CIM::TimeUtil::NowToMS();
                CIM::api::WsGatewayModule::PushToUser(current_user_id, "im.session.update",
                                                      payload);
            }
        }
    }
    result.ok = true;
    return result;
}

VoidResult MessageService::RevokeMessage(const uint64_t current_user_id, const uint8_t talk_mode,
                                         const uint64_t to_from_id, const std::string& msg_id) {
    VoidResult result;
    std::string err;

    CIM::dao::Message message;
    if (!CIM::dao::MessageDao::GetById(msg_id, message, &err)) {
        if (!err.empty()) {
            CIM_LOG_WARN(g_logger)
                << "RevokeMessage GetById error msg_id=" << msg_id << " err=" << err;
            result.code = 500;
            result.err = "消息加载失败";
            return result;
        }
    }

    // 基本权限：仅发送者可撤回
    if (message.sender_id != current_user_id) {
        result.code = 403;
        result.err = "无权限撤回";
        return result;
    }

    if (!CIM::dao::MessageDao::Revoke(msg_id, current_user_id, &err)) {
        if (!err.empty()) {
            result.code = 500;
            result.err = "撤回失败";
            return result;
        }
    }

    // 撤回成功后：若该消息为会话快照中的最后消息，则需要为受影响的用户重建/清空会话摘要
    // 先确定该消息所属的 talk_id（之前已加载到 message）
    uint64_t talk_id = message.talk_id;
    std::vector<uint64_t> affected_users;
    if (!CIM::dao::TalkSessionDAO::listUsersByLastMsg(talk_id, msg_id, affected_users, &err)) {
        CIM_LOG_WARN(g_logger) << "listUsersByLastMsg failed: " << err;
    } else {
        for (auto uid : affected_users) {
            std::vector<CIM::dao::Message> remain_msgs;
            std::string lerr;
            if (!CIM::dao::MessageDao::ListRecentDescWithFilter(talk_id, /*anchor_seq=*/0,
                                                                /*limit=*/1, uid,
                                                                /*msg_type=*/0, remain_msgs,
                                                                &lerr)) {
                CIM_LOG_WARN(g_logger)
                    << "ListRecentDescWithFilter failed for uid=" << uid << " err=" << lerr;
                continue;
            }
            if (!remain_msgs.empty()) {
                const auto& lm = remain_msgs[0];
                std::string digest;
                switch (lm.msg_type) {
                    case 1:
                        digest = lm.content_text;
                        if (digest.size() > 255) digest = digest.substr(0, 255);
                        break;
                    case 3:
                        digest = "[图片消息]";
                        break;
                    case 4:
                        digest = "[语音消息]";
                        break;
                    case 5:
                        digest = "[视频消息]";
                        break;
                    case 6:
                        digest = "[文件消息]";
                        break;
                    default:
                        digest = "[非文本消息]";
                        break;
                }
                if (!CIM::dao::TalkSessionDAO::updateLastMsgForUser(
                        uid, talk_id, std::optional<std::string>(lm.id),
                        std::optional<uint16_t>(lm.msg_type), std::optional<uint64_t>(lm.sender_id),
                        std::optional<std::string>(digest), &lerr)) {
                    CIM_LOG_WARN(g_logger)
                        << "updateLastMsgForUser failed uid=" << uid << " err=" << lerr;
                }
                // 通知客户端更新会话预览
                {
                    Json::Value payload;
                    payload["talk_mode"] = talk_mode;
                    payload["to_from_id"] = to_from_id;
                    payload["msg_text"] = digest;
                    payload["updated_at"] = (Json::UInt64)CIM::TimeUtil::NowToMS();
                    CIM::api::WsGatewayModule::PushToUser(current_user_id, "im.session.update",
                                                          payload);
                }
            } else {
                // 没有剩余消息，清空最后消息字段
                if (!CIM::dao::TalkSessionDAO::updateLastMsgForUser(
                        uid, talk_id, std::optional<std::string>(), std::optional<uint16_t>(),
                        std::optional<uint64_t>(), std::optional<std::string>(), &lerr)) {
                    CIM_LOG_WARN(g_logger)
                        << "clear last msg for user failed uid=" << uid << " err=" << lerr;
                }
                // 通知客户端清空会话预览
                {
                    Json::Value payload;
                    payload["talk_mode"] = talk_mode;
                    payload["to_from_id"] = to_from_id;
                    payload["msg_text"] = Json::Value();
                    payload["updated_at"] = (Json::UInt64)CIM::TimeUtil::NowToMS();
                    CIM::api::WsGatewayModule::PushToUser(current_user_id, "im.session.update",
                                                          payload);
                }
            }
        }
    }

    result.ok = true;
    return result;
}

MessageRecordResult MessageService::SendMessage(
    const uint64_t current_user_id, const uint8_t talk_mode, const uint64_t to_from_id,
    const uint16_t msg_type, const std::string& content_text, const std::string& extra,
    const std::string& quote_msg_id, const std::string& msg_id) {
    MessageRecordResult result;
    std::string err;

    // 1. 获取数据库连接及开启事务（自动提交关闭）。
    auto trans = CIM::MySQLMgr::GetInstance()->openTransaction(kDBName, false);
    if (!trans) {
        CIM_LOG_DEBUG(g_logger) << "SendMessage openTransaction failed, user_id="
                                << current_user_id;
        result.code = 500;
        result.err = "数据库事务创建失败";
        return result;
    }
    // 2. 获取数据库连接
    auto db = trans->getMySQL();
    if (!db) {
        CIM_LOG_DEBUG(g_logger) << "SendMessage getMySQL failed, user_id=" << current_user_id;
        result.code = 500;
        result.err = "数据库连接获取失败";
        return result;
    }

    // 3. 查或建 talk_id
    uint64_t talk_id = 0;
    if (talk_mode == 1) {
        if (!CIM::dao::TalkDao::getSingleTalkId(db, current_user_id, to_from_id, talk_id, &err)) {
            // 不存在则创建
            if (!CIM::dao::TalkDao::findOrCreateSingleTalk(db, current_user_id, to_from_id, talk_id,
                                                           &err)) {
                trans->rollback();
                result.code = 500;
                result.err = "创建单聊会话失败";
                return result;
            }
            // 如果是 ON DUPLICATE 情况 lastInsertId 可能为0 -> 再查一次确保 talk_id
            if (talk_id == 0) {
                if (!CIM::dao::TalkDao::getSingleTalkId(db, current_user_id, to_from_id, talk_id,
                                                        &err)) {
                    trans->rollback();
                    result.code = 500;
                    result.err = "获取单聊会话失败";
                    return result;
                }
            }
        }
    } else if (talk_mode == 2) {
        if (!CIM::dao::TalkDao::getGroupTalkId(db, to_from_id, talk_id, &err)) {
            // 群聊不存在则创建会话（不校验群合法性，这里假设外层已校验）
            if (!CIM::dao::TalkDao::findOrCreateGroupTalk(db, to_from_id, talk_id, &err)) {
                trans->rollback();
                result.code = 500;
                result.err = "创建群聊会话失败";
                return result;
            }
            // 再查获取 id（INSERT ON DUPLICATE KEY UPDATE 情况 lastInsertId 为0）
            if (!CIM::dao::TalkDao::getGroupTalkId(db, to_from_id, talk_id, &err)) {
                trans->rollback();
                result.code = 500;
                result.err = "获取群聊会话失败";
                return result;
            }
        }
    } else {
        trans->rollback();
        result.code = 400;
        result.err = "非法会话类型";
        return result;
    }

    // 计算下一 sequence：使用 TalkSequenceDao 在 DB 层安全分配（保证与当前事务同连接）
    uint64_t next_seq = 0;
    if (!CIM::dao::TalkSequenceDao::nextSeq(db, talk_id, next_seq, &err)) {
        trans->rollback();
        CIM_LOG_ERROR(g_logger) << "nextSeq failed, talk_id=" << talk_id << " err=" << err;
        result.code = 500;
        result.err = "分配消息序列失败";
        return result;
    }

    // 构造 DAO Message
    CIM::dao::Message m;
    m.talk_id = talk_id;
    m.sequence = next_seq;
    m.talk_mode = talk_mode;
    m.msg_type = msg_type;
    m.sender_id = current_user_id;
    if (talk_mode == 1) {
        m.receiver_id = to_from_id;
        m.group_id = 0;
    } else {
        m.receiver_id = 0;
        m.group_id = to_from_id;  // group id
    }
    m.content_text = content_text;  // 文本类存这里，其它类型留空
    m.extra = extra;                // 非文本 JSON 或补充字段
    m.quote_msg_id = quote_msg_id;
    m.is_revoked = 2;  // 正常
    m.revoke_by = 0;
    m.revoke_time = 0;
    // 使用前端传入的消息ID；若为空则服务端生成一个16进制32位随机字符串
    if (msg_id.empty()) {
        // 随机生成 32 长度 hex id
        m.id = CIM::random_string(32, "0123456789abcdef");
    } else {
        m.id = msg_id;
    }

    if (!CIM::dao::MessageDao::Create(db, m, &err)) {
        trans->rollback();
        result.code = 500;
        result.err = "消息写入失败";
        return result;
    }

    // 若为转发消息，记录转发原始消息映射表（im_message_forward_map）
    if (m.msg_type == 9 && !m.extra.empty()) {
        // extra 在 API 层已被写成 JSON 字符串
        Json::CharReaderBuilder rb;
        Json::Value payload;
        std::string errs;
        std::istringstream in(m.extra);
        if (Json::parseFromStream(rb, in, &payload, &errs)) {
            std::vector<std::string> src_ids;
            if (payload.isMember("msg_ids") && payload["msg_ids"].isArray()) {
                for (auto& v : payload["msg_ids"]) {
                    if (v.isString()) {
                        src_ids.push_back(v.asString());
                    } else if (v.isUInt64()) {
                        src_ids.push_back(std::to_string(v.asUInt64()));
                    }
                }
            }

            if (!src_ids.empty()) {
                std::vector<CIM::dao::Message> src_msgs;
                if (CIM::dao::MessageDao::GetByIds(src_ids, src_msgs, &err)) {
                    std::vector<CIM::dao::ForwardSrc> srcs;
                    for (auto& s : src_msgs) {
                        CIM::dao::ForwardSrc fs;
                        fs.src_msg_id = s.id;
                        fs.src_talk_id = s.talk_id;
                        fs.src_sender_id = s.sender_id;
                        srcs.push_back(std::move(fs));
                    }
                    if (!CIM::dao::MessageForwardMapDao::AddForwardMap(db, m.id, srcs, &err)) {
                        CIM_LOG_WARN(g_logger) << "AddForwardMap failed: " << err;
                        // 非关键业务，继续处理并返回成功消息发送
                    }
                } else {
                    CIM_LOG_WARN(g_logger) << "MessageDao::GetByIds failed: " << err;
                }
            }
        } else {
            CIM_LOG_WARN(g_logger) << "Parse forward extra payload failed: " << errs;
        }
    }

    // 生成最后一条消息摘要（用于会话列表预览文案）并更新会话表的 last_msg_* 字段
    std::string last_msg_digest;
    switch (m.msg_type) {
        case 1:  // 文本
            last_msg_digest = m.content_text;
            if (last_msg_digest.size() > 255) last_msg_digest = last_msg_digest.substr(0, 255);
            break;
        case 3:  // 图片
            last_msg_digest = "[图片消息]";
            break;
        case 4:  // 语音
            last_msg_digest = "[语音消息]";
            break;
        case 5:  // 视频
            last_msg_digest = "[视频消息]";
            break;
        case 6:  // 文件
            last_msg_digest = "[文件消息]";
            break;
        default:
            last_msg_digest = "[非文本消息]";
            break;
    }

    // 在同一事务连接中更新会话的最后消息信息，保证会话列表能及时显示预览
    if (!CIM::dao::TalkSessionDAO::bumpOnNewMessage(db, talk_id, current_user_id, m.id,
                                                    static_cast<uint16_t>(m.msg_type),
                                                    last_msg_digest, &err)) {
        trans->rollback();
        CIM_LOG_ERROR(g_logger) << "bumpOnNewMessage failed: " << err;
        result.code = 500;
        result.err = "更新会话摘要失败";
        return result;
    }

    if (!trans->commit()) {
        const auto commit_err = db->getErrStr();
        trans->rollback();
        CIM_LOG_ERROR(g_logger) << "Transaction commit failed: " << commit_err;
        result.code = 500;
        result.err = "事务提交失败";
        return result;
    }

    // 构建返回记录（补充昵称头像与引用信息）
    CIM::dao::MessageRecord rec;
    buildRecord(m, rec, &err);
    result.data = std::move(rec);
    result.ok = true;
    return result;
}

}  // namespace CIM::app
