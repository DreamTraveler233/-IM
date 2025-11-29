#include "app/talk_service_impl.hpp"

#include "base/macro.hpp"
#include "dto/contact_dto.hpp"

namespace IM::app {

static auto g_logger = IM_LOG_NAME("root");
static constexpr const char* kDBName = "default";

TalkServiceImpl::TalkServiceImpl(IM::domain::repository::ITalkRepository::Ptr talk_repo,
                                 IM::domain::repository::IContactRepository::Ptr contact_repo,
                                 IM::domain::repository::IMessageRepository::Ptr message_repo,
                                 IM::domain::repository::IGroupRepository::Ptr group_repo)
    : m_talk_repo(std::move(talk_repo)),
      m_contact_repo(std::move(contact_repo)),
      m_message_repo(std::move(message_repo)),
      m_group_repo(std::move(group_repo)) {}

Result<std::vector<dto::TalkSessionItem>> TalkServiceImpl::getSessionListByUserId(
    const uint64_t user_id) {
    Result<std::vector<dto::TalkSessionItem>> result;
    std::string err;

    if (!m_talk_repo->getSessionListByUserId(user_id, result.data, &err)) {
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger)
                << "TalkServiceImpl::getSessionListByUserId failed, user_id=" << user_id
                << ", err=" << err;
            result.code = 500;
            result.err = "获取会话列表失败";
            return result;
        }
    }

    result.ok = true;
    return result;
}

Result<void> TalkServiceImpl::setSessionTop(const uint64_t user_id, const uint64_t to_from_id,
                                            const uint8_t talk_mode, const uint8_t action) {
    Result<void> result;
    std::string err;

    if (!m_talk_repo->setSessionTop(user_id, to_from_id, talk_mode, action, &err)) {
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger)
                << "TalkServiceImpl::setSessionTop failed, to_from_id=" << to_from_id
                << ", talk_mode=" << talk_mode << ", action=" << action << ", err=" << err;
            result.code = 500;
            result.err = "设置会话置顶失败";
            return result;
        }
    }

    result.ok = true;
    return result;
}

Result<void> TalkServiceImpl::setSessionDisturb(const uint64_t user_id, const uint64_t to_from_id,
                                                const uint8_t talk_mode, const uint8_t action) {
    Result<void> result;
    std::string err;

    if (!m_talk_repo->setSessionDisturb(user_id, to_from_id, talk_mode, action, &err)) {
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger)
                << "TalkServiceImpl::setSessionDisturb failed, to_from_id=" << to_from_id
                << ", talk_mode=" << talk_mode << ", action=" << action << ", err=" << err;
            result.code = 500;
            result.err = "设置会话免打扰失败";
            return result;
        }
    }

    result.ok = true;
    return result;
}

Result<dto::TalkSessionItem> TalkServiceImpl::createSession(const uint64_t user_id,
                                                            const uint64_t to_from_id,
                                                            const uint8_t talk_mode) {
    Result<dto::TalkSessionItem> result;
    std::string err;

    // 1. 开启数据库事务，保证后续操作的原子性
    auto trans = IM::MySQLMgr::GetInstance()->openTransaction(kDBName, false);
    if (!trans) {
        IM_LOG_ERROR(g_logger) << "createSession openTransaction failed, user_id=" << user_id;
        result.code = 500;
        result.err = "创建会话失败";
        return result;
    }

    // 2. 获取事务绑定的数据库连接
    auto db = trans->getMySQL();
    if (!db) {
        IM_LOG_ERROR(g_logger) << "createSession get transaction connection failed, user_id="
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
        if (!m_talk_repo->findOrCreateSingleTalk(db, user_id, to_from_id, talk_id, &err)) {
            trans->rollback();
            IM_LOG_ERROR(g_logger)
                << "createSession findOrCreateSingleTalk failed, user_id=" << user_id
                << ", to_from_id=" << to_from_id << ", err=" << err;
            result.code = 500;
            result.err = "创建会话失败";
            return result;
        }
    } else if (talk_mode == 2) {  // 群聊
        if (!m_talk_repo->findOrCreateGroupTalk(db, to_from_id, talk_id, &err)) {
            trans->rollback();
            IM_LOG_ERROR(g_logger)
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
    IM::dto::ContactDetails cd;
    if (!m_contact_repo->GetByOwnerAndTarget(db, user_id, to_from_id, cd, &err)) {
        trans->rollback();  // 回滚事务
        IM_LOG_ERROR(g_logger)
            << "TalkServiceImpl::createSession GetByOwnerAndTargetWithConn failed, user_id="
            << user_id << ", to_from_id=" << to_from_id
            << ", talk_mode=" << static_cast<int>(talk_mode) << ", err=" << err;
        result.code = 500;
        result.err = "创建会话失败";
        return result;
    }


    IM::model::TalkSession session;
    session.user_id = user_id;
    session.talk_id = talk_id;
    session.to_from_id = to_from_id;
    session.talk_mode = talk_mode;
    if (talk_mode == 1) {
        session.name = cd.nickname;
        session.avatar = cd.avatar;
        session.remark = cd.contact_remark;
    }else if (talk_mode == 2) {
    // 获取群组信息
        IM::model::Group group;
        if (!m_group_repo->GetGroupById(db, to_from_id, group, &err)) {
            trans->rollback();  // 回滚事务
            IM_LOG_ERROR(g_logger)
                << "TalkServiceImpl::createSession GetGroupById failed, user_id=" << user_id
                << ", group_id=" << to_from_id
                << ", err=" << err;
            result.code = 500;
            result.err = "创建会话失败";
            return result;  
        }
        session.name = group.group_name;
        session.avatar = group.avatar;
    }
    if (!m_talk_repo->createSession(db, session, &err)) {
        trans->rollback();  // 回滚事务
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger)
                << "TalkServiceImpl::createSession failed, user_id=" << user_id
                << ", to_from_id=" << to_from_id << ", talk_mode=" << static_cast<int>(talk_mode)
                << ", err=" << err;
            result.code = 500;
            result.err = "创建会话失败";
        }
    }

    // 5. 获取会话信息
    if (!m_talk_repo->getSessionByUserId(db, user_id, result.data, to_from_id, talk_mode, &err)) {
        trans->rollback();  // 回滚事务
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger)
                << "TalkServiceImpl::createSession getSessionByUserId failed, user_id=" << user_id
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
        IM_LOG_ERROR(g_logger)
            << "TalkServiceImpl::createSession commit transaction failed, user_id=" << user_id
            << ", err=" << commit_err;
        result.code = 500;
        result.err = "创建会话失败";
        return result;
    }

    result.ok = true;
    return result;
}

Result<void> TalkServiceImpl::deleteSession(const uint64_t user_id, const uint64_t to_from_id,
                                            const uint8_t talk_mode) {
    Result<void> result;
    std::string err;

    if (!m_talk_repo->deleteSession(user_id, to_from_id, talk_mode, &err)) {
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger)
                << "TalkServiceImpl::deleteSession failed, user_id=" << user_id
                << ", to_from_id=" << to_from_id << ", talk_mode=" << talk_mode << ", err=" << err;
            result.code = 500;
            result.err = "删除会话失败";
            return result;
        }
    }

    result.ok = true;
    return result;
}

Result<void> TalkServiceImpl::clearSessionUnreadNum(const uint64_t user_id,
                                                    const uint64_t to_from_id,
                                                    const uint8_t talk_mode) {
    Result<void> result;
    std::string err;

    if (!m_talk_repo->clearSessionUnreadNum(user_id, to_from_id, talk_mode, &err)) {
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger)
                << "TalkServiceImpl::clearSessionUnreadNum failed, user_id=" << user_id
                << ", to_from_id=" << to_from_id << ", talk_mode=" << talk_mode << ", err=" << err;
            result.code = 500;
            result.err = "清除会话未读消息数失败";
            return result;
        }
    }
    // 同时将该会话已读状态写入 im_message_read 表
    uint64_t talk_id = 0;
    if (talk_mode == 1) {
        if (m_talk_repo->getSingleTalkId(user_id, to_from_id, talk_id, &err)) {
            (void)m_message_repo->MarkReadByTalk(talk_id, user_id, &err);
        }
    } else if (talk_mode == 2) {
        if (m_talk_repo->getGroupTalkId(to_from_id, talk_id, &err)) {
            (void)m_message_repo->MarkReadByTalk(talk_id, user_id, &err);
        }
    }

    result.ok = true;
    return result;
}

}  // namespace IM::app