#include "app/talk_service.hpp"

#include "dao/talk_dao.hpp"
#include "dao/talk_session_dao.hpp"
#include "dao/user_dao.hpp"
#include "base/macro.hpp"

namespace CIM::app {

static auto g_logger = CIM_LOG_NAME("root");
static constexpr const char* kDBName = "default";

TalkSessionListResult TalkService::getSessionListByUserId(const uint64_t user_id) {
    TalkSessionListResult result;
    std::string err;

    if (!dao::TalkSessionDAO::getSessionListByUserId(user_id, result.data, &err)) {
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger)
                << "TalkService::getSessionListByUserId failed, user_id=" << user_id
                << ", err=" << err;
            result.code = 500;
            result.err = "获取会话列表失败";
            return result;
        }
    }

    result.ok = true;
    return result;
}

VoidResult TalkService::setSessionTop(const uint64_t user_id, const uint64_t to_from_id,
                                      const uint8_t talk_mode, const uint8_t action) {
    VoidResult result;
    std::string err;

    if (!dao::TalkSessionDAO::setSessionTop(user_id, to_from_id, talk_mode, action, &err)) {
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger)
                << "TalkService::setSessionTop failed, to_from_id=" << to_from_id
                << ", talk_mode=" << talk_mode << ", action=" << action << ", err=" << err;
            result.code = 500;
            result.err = "设置会话置顶失败";
            return result;
        }
    }

    result.ok = true;
    return result;
}

VoidResult TalkService::setSessionDisturb(const uint64_t user_id, const uint64_t to_from_id,
                                          const uint8_t talk_mode, const uint8_t action) {
    VoidResult result;
    std::string err;

    if (!dao::TalkSessionDAO::setSessionDisturb(user_id, to_from_id, talk_mode, action, &err)) {
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger)
                << "TalkService::setSessionDisturb failed, to_from_id=" << to_from_id
                << ", talk_mode=" << talk_mode << ", action=" << action << ", err=" << err;
            result.code = 500;
            result.err = "设置会话免打扰失败";
            return result;
        }
    }

    result.ok = true;
    return result;
}

TalkSessionResult TalkService::createSession(const uint64_t user_id, const uint64_t to_from_id,
                                             const uint8_t talk_mode) {
    TalkSessionResult result;
    std::string err;

    // 1. 开启数据库事务，保证后续操作的原子性
    auto trans = CIM::MySQLMgr::GetInstance()->openTransaction(kDBName, false);
    if (!trans) {
        CIM_LOG_ERROR(g_logger) << "createSession openTransaction failed, user_id=" << user_id;
        result.code = 500;
        result.err = "创建会话失败";
        return result;
    }

    // 2. 获取事务绑定的数据库连接
    auto db = trans->getMySQL();
    if (!db) {
        CIM_LOG_ERROR(g_logger) << "createSession get transaction connection failed, user_id="
                                << user_id;
        result.code = 500;
        result.err = "创建会话失败";
        return result;
    }

    // 3. 获取/创建 talk 主实体
    uint64_t talk_id = 0;
    if (talk_mode == 1) {  // 单聊
        if (user_id == to_from_id) {
            result.code = 400;
            result.err = "不能与自己创建单聊会话";
            return result;
        }
        if (!CIM::dao::TalkDao::findOrCreateSingleTalk(db, user_id, to_from_id, talk_id, &err)) {
            trans->rollback();
            CIM_LOG_ERROR(g_logger)
                << "createSession findOrCreateSingleTalk failed, user_id=" << user_id
                << ", to_from_id=" << to_from_id << ", err=" << err;
            result.code = 500;
            result.err = "创建会话失败";
            return result;
        }
    } else if (talk_mode == 2) {  // 群聊
        if (!CIM::dao::TalkDao::findOrCreateGroupTalk(db, to_from_id, talk_id, &err)) {
            trans->rollback();
            CIM_LOG_ERROR(g_logger)
                << "createSession findOrCreateGroupTalk failed, user_id=" << user_id
                << ", group_id=" << to_from_id << ", err=" << err;
            result.code = 500;
            result.err = "创建会话失败";
            return result;
        }
    } else {
        result.code = 400;
        result.err = "非法会话类型";
        return result;
    }

    // 4. 创建/恢复个人会话视图（upsert）
    CIM::dao::ContactDetails cd;
    if (!CIM::dao::ContactDAO::GetByOwnerAndTargetWithConn(db, user_id, to_from_id, cd, &err)) {
        trans->rollback();  // 回滚事务
        CIM_LOG_ERROR(g_logger)
            << "TalkService::createSession GetByOwnerAndTargetWithConn failed, user_id=" << user_id
            << ", to_from_id=" << to_from_id << ", talk_mode=" << static_cast<int>(talk_mode)
            << ", err=" << err;
        result.code = 500;
        result.err = "创建会话失败";
        return result;
    }

    CIM::dao::TalkSession session;
    session.user_id = user_id;
    session.talk_id = talk_id;
    session.to_from_id = to_from_id;
    session.talk_mode = talk_mode;
    if (talk_mode == 1) {
        session.name = cd.nickname;
        session.avatar = cd.avatar;
        session.remark = cd.contact_remark;
    }
    if (!dao::TalkSessionDAO::createSession(db, session, &err)) {
        trans->rollback();  // 回滚事务
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger)
                << "TalkService::createSession failed, user_id=" << user_id
                << ", to_from_id=" << to_from_id << ", talk_mode=" << static_cast<int>(talk_mode)
                << ", err=" << err;
            result.code = 500;
            result.err = "创建会话失败";
        }
    }

    // 5. 获取会话信息
    if (!dao::TalkSessionDAO::getSessionByUserId(db, user_id, result.data, to_from_id, talk_mode,
                                                 &err)) {
        trans->rollback();  // 回滚事务
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger)
                << "TalkService::createSession getSessionByUserId failed, user_id=" << user_id
                << ", to_from_id=" << to_from_id << ", talk_mode=" << static_cast<int>(talk_mode)
                << ", err=" << err;
            result.code = 500;
            result.err = "获取会话信息失败";
        }
    }

    // 6. 提交事务
    if (!trans->commit()) {
        // 提交失败也要回滚，保证数据一致性
        const auto commit_err = db->getErrStr();
        trans->rollback();
        CIM_LOG_ERROR(g_logger) << "TalkService::createSession commit transaction failed, user_id="
                                << user_id << ", err=" << commit_err;
        result.code = 500;
        result.err = "创建会话失败";
        return result;
    }

    result.ok = true;
    return result;
}

VoidResult TalkService::deleteSession(const uint64_t user_id, const uint64_t to_from_id,
                                      const uint8_t talk_mode) {
    VoidResult result;
    std::string err;

    if (!dao::TalkSessionDAO::deleteSession(user_id, to_from_id, talk_mode, &err)) {
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger)
                << "TalkService::deleteSession failed, user_id=" << user_id
                << ", to_from_id=" << to_from_id << ", talk_mode=" << talk_mode << ", err=" << err;
            result.code = 500;
            result.err = "删除会话失败";
            return result;
        }
    }

    result.ok = true;
    return result;
}

VoidResult TalkService::clearSessionUnreadNum(const uint64_t user_id, const uint64_t to_from_id,
                                              const uint8_t talk_mode) {
    VoidResult result;
    std::string err;

    if (!dao::TalkSessionDAO::clearSessionUnreadNum(user_id, to_from_id, talk_mode, &err)) {
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger)
                << "TalkService::clearSessionUnreadNum failed, user_id=" << user_id
                << ", to_from_id=" << to_from_id << ", talk_mode=" << talk_mode << ", err=" << err;
            result.code = 500;
            result.err = "清除会话未读消息数失败";
            return result;
        }
    }

    result.ok = true;
    return result;
}

}  // namespace CIM::app