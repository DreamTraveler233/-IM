#include "infra/repository/talk_repository_impl.hpp"

#include "util/time_util.hpp"

namespace IM::infra::repository {

static constexpr const char* kDBName = "default";

TalkRepositoryImpl::TalkRepositoryImpl(std::shared_ptr<IM::MySQLManager> db_manager)
    : m_db_manager(std::move(db_manager)) {}

namespace {
inline void order_pair(uint64_t a, uint64_t b, uint64_t& mn, uint64_t& mx) {
    if (a <= b) {
        mn = a;
        mx = b;
    } else {
        mn = b;
        mx = a;
    }
}
}  // namespace

bool TalkRepositoryImpl::findOrCreateSingleTalk(const std::shared_ptr<IM::MySQL>& db, uint64_t uid1,
                                                uint64_t uid2, uint64_t& out_talk_id,
                                                std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    uint64_t umin = 0, umax = 0;
    order_pair(uid1, uid2, umin, umax);
    const char* sql =
        "INSERT INTO im_talk (talk_mode, user_min_id, user_max_id, created_at, updated_at) "
        "VALUES (1, ?, ?, NOW(), NOW()) "
        "ON DUPLICATE KEY UPDATE updated_at=VALUES(updated_at), id=LAST_INSERT_ID(id)";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, umin);
    stmt->bindUint64(2, umax);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    out_talk_id = stmt->getLastInsertId();
    return true;
}

bool TalkRepositoryImpl::findOrCreateGroupTalk(const std::shared_ptr<IM::MySQL>& db,
                                               uint64_t group_id, uint64_t& out_talk_id,
                                               std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "INSERT INTO im_talk (talk_mode, group_id, created_at, updated_at) "
        "VALUES (2, ?, NOW(), NOW()) "
        "ON DUPLICATE KEY UPDATE updated_at=VALUES(updated_at), id=LAST_INSERT_ID(id)";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, group_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    out_talk_id = stmt->getLastInsertId();
    return true;
}

bool TalkRepositoryImpl::getSingleTalkId(const uint64_t uid1, const uint64_t uid2,
                                         uint64_t& out_talk_id, std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    uint64_t umin = 0, umax = 0;
    order_pair(uid1, uid2, umin, umax);

    const char* sql =
        "SELECT id FROM im_talk WHERE talk_mode=1 AND user_min_id=? AND user_max_id=? LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, umin);
    stmt->bindUint64(2, umax);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    if (!res->next()) {
        // 不存在不视为错误，返回 false；由调用方决定是否创建
        return false;
    }
    out_talk_id = res->getUint64(0);
    return true;
}

bool TalkRepositoryImpl::getGroupTalkId(const uint64_t group_id, uint64_t& out_talk_id,
                                        std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }

    const char* sql = "SELECT id FROM im_talk WHERE talk_mode=2 AND group_id=? LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, group_id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    if (!res->next()) {
        // 不存在不视为错误，返回 false；由调用方决定是否创建
        return false;
    }
    out_talk_id = res->getUint64(0);
    return true;
}

bool TalkRepositoryImpl::nextSeq(const std::shared_ptr<IM::MySQL>& db, uint64_t talk_id,
                                 uint64_t& seq, std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }

    // Upsert + 自增
    {
        const char* sql =
            "INSERT INTO im_talk_sequence (talk_id, last_seq, created_at, updated_at) "
            "VALUES (?, 1, NOW(), NOW()) "
            "ON DUPLICATE KEY UPDATE last_seq = last_seq + 1, updated_at = NOW()";
        auto stmt = db->prepare(sql);
        if (!stmt) {
            if (err) *err = "prepare sql failed";
            return false;
        }
        stmt->bindUint64(1, talk_id);
        if (stmt->execute() != 0) {
            if (err) *err = stmt->getErrStr();
            return false;
        }
    }

    // 查询最新序列
    {
        const char* sql = "SELECT last_seq FROM im_talk_sequence WHERE talk_id = ? LIMIT 1";
        auto stmt = db->prepare(sql);
        if (!stmt) {
            if (err) *err = "prepare sql failed";
            return false;
        }
        stmt->bindUint64(1, talk_id);
        auto res = stmt->query();
        if (!res) {
            if (err) *err = std::string("query failed: ") + db->getErrStr();
            return false;
        }
        if (!res->next()) {
            if (err) *err = "no session found";
            return false;
        }
        seq = res->getUint64(0);
    }

    return true;
}

bool TalkRepositoryImpl::getSessionListByUserId(const uint64_t user_id,
                                                std::vector<dto::TalkSessionItem>& out,
                                                std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT t.id, t.talk_mode, ts.to_from_id, ts.is_top, ts.is_disturb, ts.is_robot, "
        "ts.name, ts.avatar, ts.remark, ts.unread_num, ts.last_msg_digest, "
        "ts.updated_at "
        "FROM im_talk_session ts LEFT JOIN im_talk t ON ts.talk_id = t.id "
        "WHERE ts.user_id = ? AND ts.deleted_at IS NULL "
        "ORDER BY ts.is_top DESC, ts.updated_at DESC";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, user_id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    while (res->next()) {
        dto::TalkSessionItem item;
        item.id = res->getUint64(0);
        item.talk_mode = res->getUint8(1);
        item.to_from_id = res->getUint64(2);
        item.is_top = res->getUint8(3);
        item.is_disturb = res->getUint8(4);
        item.is_robot = res->getUint8(5);
        item.name = res->isNull(6) ? std::string() : res->getString(6);
        item.avatar = res->isNull(7) ? std::string() : res->getString(7);
        item.remark = res->isNull(8) ? std::string() : res->getString(8);
        item.unread_num = res->getUint32(9);
        item.msg_text = res->isNull(10) ? std::string() : res->getString(10);
        item.updated_at = TimeUtil::TimeToStr(res->getTime(11));
        out.push_back(item);
    }
    return true;
}

bool TalkRepositoryImpl::setSessionTop(const uint64_t user_id, const uint64_t to_from_id,
                                       const uint8_t talk_mode, const uint8_t action,
                                       std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "UPDATE im_talk_session SET is_top = ? "
        "WHERE to_from_id = ? AND talk_mode = ? AND user_id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint8(1, action);
    stmt->bindUint64(2, to_from_id);
    stmt->bindUint8(3, talk_mode);
    stmt->bindUint64(4, user_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool TalkRepositoryImpl::setSessionDisturb(const uint64_t user_id, const uint64_t to_from_id,
                                           const uint8_t talk_mode, const uint8_t action,
                                           std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "UPDATE im_talk_session SET is_disturb = ? "
        "WHERE to_from_id = ? AND talk_mode = ? AND user_id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint8(1, action);
    stmt->bindUint64(2, to_from_id);
    stmt->bindUint8(3, talk_mode);
    stmt->bindUint64(4, user_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}
bool TalkRepositoryImpl::createSession(const std::shared_ptr<IM::MySQL>& db,
                                       const model::TalkSession& session, std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "INSERT INTO im_talk_session (user_id, talk_id, to_from_id, talk_mode, is_top, is_disturb, "
        "is_robot, name, avatar, remark, last_ack_seq, last_msg_id, last_msg_type, last_sender_id, "
        "draft_text, unread_num, last_msg_digest, created_at, updated_at, "
        "deleted_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, NOW(), NOW(), ?) "
        "ON DUPLICATE KEY UPDATE deleted_at=NULL, updated_at=NOW()";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, session.user_id);
    stmt->bindUint64(2, session.talk_id);
    stmt->bindUint64(3, session.to_from_id);
    stmt->bindUint8(4, session.talk_mode);
    stmt->bindUint8(5, session.is_top);
    stmt->bindUint8(6, session.is_disturb);
    stmt->bindUint8(7, session.is_robot);
    if (session.name.has_value()) {
        stmt->bindString(8, *session.name);
    } else {
        stmt->bindNull(8);
    }
    if (session.avatar.has_value()) {
        stmt->bindString(9, *session.avatar);
    } else {
        stmt->bindNull(9);
    }
    if (session.remark.has_value()) {
        stmt->bindString(10, *session.remark);
    } else {
        stmt->bindNull(10);
    }
    stmt->bindUint64(11, session.last_ack_seq);
    if (session.last_msg_id.has_value()) {
        stmt->bindString(12, *session.last_msg_id);
    } else {
        stmt->bindNull(12);
    }
    if (session.last_msg_type.has_value()) {
        stmt->bindUint16(13, *session.last_msg_type);
    } else {
        stmt->bindNull(13);
    }
    if (session.last_sender_id.has_value()) {
        stmt->bindUint64(14, *session.last_sender_id);
    } else {
        stmt->bindNull(14);
    }
    if (session.draft_text.has_value()) {
        stmt->bindString(15, *session.draft_text);
    } else {
        stmt->bindNull(15);
    }
    stmt->bindUint32(16, session.unread_num);
    if (session.last_msg_digest.has_value()) {
        stmt->bindString(17, *session.last_msg_digest);
    } else {
        stmt->bindNull(17);
    }
    if (session.deleted_at.has_value()) {
        stmt->bindTime(18, *session.deleted_at);
    } else {
        stmt->bindNull(18);
    }
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool TalkRepositoryImpl::bumpOnNewMessage(const std::shared_ptr<IM::MySQL>& db,
                                          const uint64_t talk_id, const uint64_t sender_user_id,
                                          const std::string& last_msg_id,
                                          const uint16_t last_msg_type,
                                          const std::string& last_msg_digest, std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "UPDATE im_talk_session SET last_msg_id = ?, last_msg_type = ?, last_sender_id = ?, "
        "last_msg_digest = ?, updated_at = NOW(), "
        "unread_num = CASE WHEN user_id <> ? THEN unread_num + 1 ELSE unread_num END "
        "WHERE talk_id = ? AND deleted_at IS NULL";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindString(1, last_msg_id);
    stmt->bindUint16(2, last_msg_type);
    stmt->bindUint64(3, sender_user_id);
    stmt->bindString(4, last_msg_digest);
    stmt->bindUint64(5, sender_user_id);
    stmt->bindUint64(6, talk_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool TalkRepositoryImpl::editRemarkWithConn(const std::shared_ptr<IM::MySQL>& db,
                                            const uint64_t user_id, const uint64_t to_from_id,
                                            const std::string& remark, std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "UPDATE im_talk_session SET remark = ? WHERE user_id = ? AND to_from_id = ? AND deleted_at "
        "IS NULL";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindString(1, remark);
    stmt->bindUint64(2, user_id);
    stmt->bindUint64(3, to_from_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool TalkRepositoryImpl::updateSessionAvatarByTargetUser(const uint64_t target_user_id,
                                                        const std::string& avatar,
                                                        std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "UPDATE im_talk_session SET avatar = ? WHERE talk_mode = 1 AND to_from_id = ? AND deleted_at IS NULL";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    if (avatar.empty()) {
        stmt->bindNull(1);
    } else {
        stmt->bindString(1, avatar);
    }
    stmt->bindUint64(2, target_user_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool TalkRepositoryImpl::updateSessionAvatarByTargetUserWithConn(const std::shared_ptr<IM::MySQL>& db,
                                                                const uint64_t target_user_id,
                                                                const std::string& avatar,
                                                                std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "UPDATE im_talk_session SET avatar = ? WHERE talk_mode = 1 AND to_from_id = ? AND deleted_at IS NULL";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    if (avatar.empty()) {
        stmt->bindNull(1);
    } else {
        stmt->bindString(1, avatar);
    }
    stmt->bindUint64(2, target_user_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool TalkRepositoryImpl::listUsersByTargetUserWithConn(const std::shared_ptr<IM::MySQL>& db,
                                                      const uint64_t target_user_id,
                                                      std::vector<uint64_t>& out_user_ids,
                                                      std::string* err) {
    out_user_ids.clear();
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT user_id FROM im_talk_session WHERE talk_mode = 1 AND to_from_id = ? AND deleted_at IS NULL";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, target_user_id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    while (res->next()) {
        out_user_ids.push_back(res->getUint64(0));
    }
    return true;
}

bool TalkRepositoryImpl::getSessionByUserId(const std::shared_ptr<IM::MySQL>& db,
                                            const uint64_t user_id, dto::TalkSessionItem& out,
                                            const uint64_t to_from_id, const uint8_t talk_mode,
                                            std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT t.id, t.talk_mode, ts.to_from_id, ts.is_top, ts.is_disturb, ts.is_robot, "
        "ts.name, ts.avatar, ts.remark, ts.unread_num, ts.last_msg_digest, "
        "ts.updated_at "
        "FROM im_talk_session ts LEFT JOIN im_talk t ON ts.talk_id = t.id "
        "WHERE ts.user_id = ? AND ts.talk_mode = ? AND ts.to_from_id = ? AND ts.deleted_at IS NULL "
        "LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, user_id);
    stmt->bindUint8(2, talk_mode);
    stmt->bindUint64(3, to_from_id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    if (!res->next()) {
        if (err) *err = "no session found";
        return false;
    }

    out.id = res->getUint64(0);
    out.talk_mode = res->getUint8(1);
    out.to_from_id = res->getUint64(2);
    out.is_top = res->getUint8(3);
    out.is_disturb = res->getUint8(4);
    out.is_robot = res->getUint8(5);
    out.name = res->isNull(6) ? std::string() : res->getString(6);
    out.avatar = res->isNull(7) ? std::string() : res->getString(7);
    out.remark = res->isNull(8) ? std::string() : res->getString(8);
    out.unread_num = res->getUint32(9);
    out.msg_text = res->isNull(10) ? std::string() : res->getString(10);
    out.updated_at = TimeUtil::TimeToStr(res->getTime(11));

    return true;
}
bool TalkRepositoryImpl::deleteSession(const uint64_t user_id, const uint64_t to_from_id,
                                       const uint8_t talk_mode, std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "UPDATE im_talk_session SET deleted_at = NOW() "
        "WHERE user_id = ? AND to_from_id = ? AND talk_mode = ? AND deleted_at IS NULL";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, user_id);
    stmt->bindUint64(2, to_from_id);
    stmt->bindUint8(3, talk_mode);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool TalkRepositoryImpl::deleteSession(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                                       const uint64_t to_from_id, const uint8_t talk_mode,
                                       std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "UPDATE im_talk_session SET deleted_at = NOW() "
        "WHERE user_id = ? AND to_from_id = ? AND talk_mode = ? AND deleted_at IS NULL";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, user_id);
    stmt->bindUint64(2, to_from_id);
    stmt->bindUint8(3, talk_mode);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool TalkRepositoryImpl::clearSessionUnreadNum(const uint64_t user_id, const uint64_t to_from_id,
                                               const uint8_t talk_mode, std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "UPDATE im_talk_session SET unread_num = 0 "
        "WHERE user_id = ? AND to_from_id = ? AND talk_mode = ? AND deleted_at IS NULL";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, user_id);
    stmt->bindUint64(2, to_from_id);
    stmt->bindUint8(3, talk_mode);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool TalkRepositoryImpl::updateLastMsgForUser(const std::shared_ptr<IM::MySQL>& db,
                                              const uint64_t user_id, const uint64_t talk_id,
                                              const std::optional<std::string>& last_msg_id,
                                              const std::optional<uint16_t>& last_msg_type,
                                              const std::optional<uint64_t>& last_sender_id,
                                              const std::optional<std::string>& last_msg_digest,
                                              std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }

    const char* sql =
        "UPDATE im_talk_session SET last_msg_id = ?, last_msg_type = ?, last_sender_id = ?, "
        "last_msg_digest = ?, updated_at = NOW() "
        "WHERE user_id = ? AND talk_id = ? AND deleted_at IS NULL";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }

    if (last_msg_id.has_value())
        stmt->bindString(1, *last_msg_id);
    else
        stmt->bindNull(1);

    if (last_msg_type.has_value())
        stmt->bindUint16(2, *last_msg_type);
    else
        stmt->bindNull(2);

    if (last_sender_id.has_value())
        stmt->bindUint64(3, *last_sender_id);
    else
        stmt->bindNull(3);

    if (last_msg_digest.has_value())
        stmt->bindString(4, *last_msg_digest);
    else
        stmt->bindNull(4);

    stmt->bindUint64(5, user_id);
    stmt->bindUint64(6, talk_id);

    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool TalkRepositoryImpl::listUsersByLastMsg(const std::shared_ptr<IM::MySQL>& db,
                                            const uint64_t talk_id, const std::string& last_msg_id,
                                            std::vector<uint64_t>& out_user_ids, std::string* err) {
    out_user_ids.clear();
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }

    const char* sql =
        "SELECT user_id FROM im_talk_session WHERE talk_id = ? AND last_msg_id = ? AND deleted_at "
        "IS NULL";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, talk_id);
    stmt->bindString(2, last_msg_id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    while (res->next()) {
        out_user_ids.push_back(res->getUint64(0));
    }
    return true;
}

bool TalkRepositoryImpl::listUsersByTalkId(const uint64_t talk_id,
                                           std::vector<uint64_t>& out_user_ids, std::string* err) {
    out_user_ids.clear();
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }

    const char* sql =
        "SELECT user_id FROM im_talk_session WHERE talk_id = ? AND deleted_at IS NULL";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, talk_id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    while (res->next()) {
        out_user_ids.push_back(res->getUint64(0));
    }
    return true;
}

}  // namespace IM::infra::repository