#include "dao/talk_session_dao.hpp"

#include "db/mysql.hpp"
#include "util/time_util.hpp"

namespace CIM::dao {

static constexpr const char* kDBName = "default";

bool TalkSessionDAO::getSessionListByUserId(const uint64_t user_id,
                                            std::vector<TalkSessionItem>& out, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
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
        TalkSessionItem item;
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

bool TalkSessionDAO::setSessionTop(const uint64_t user_id, const uint64_t to_from_id,
                                   const uint8_t talk_mode, const uint8_t action,
                                   std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
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

bool TalkSessionDAO::setSessionDisturb(const uint64_t user_id, const uint64_t to_from_id,
                                       const uint8_t talk_mode, const uint8_t action,
                                       std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
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
bool TalkSessionDAO::createSession(const std::shared_ptr<CIM::MySQL>& db,
                                   const TalkSession& session, std::string* err) {
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

bool TalkSessionDAO::bumpOnNewMessage(const std::shared_ptr<CIM::MySQL>& db, const uint64_t talk_id,
                                      const uint64_t sender_user_id, const std::string& last_msg_id,
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

bool TalkSessionDAO::EditRemarkWithConn(const std::shared_ptr<CIM::MySQL>& db,
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

bool TalkSessionDAO::getSessionByUserId(const std::shared_ptr<CIM::MySQL>& db,
                                        const uint64_t user_id, TalkSessionItem& out,
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
bool TalkSessionDAO::deleteSession(const uint64_t user_id, const uint64_t to_from_id,
                                   const uint8_t talk_mode, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
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
bool TalkSessionDAO::clearSessionUnreadNum(const uint64_t user_id, const uint64_t to_from_id,
                                           const uint8_t talk_mode, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
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

bool TalkSessionDAO::updateLastMsgForUser(const uint64_t user_id, const uint64_t talk_id,
                                          const std::optional<std::string>& last_msg_id,
                                          const std::optional<uint16_t>& last_msg_type,
                                          const std::optional<uint64_t>& last_sender_id,
                                          const std::optional<std::string>& last_msg_digest,
                                          std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
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

bool TalkSessionDAO::listUsersByLastMsg(const uint64_t talk_id, const std::string& last_msg_id,
                                        std::vector<uint64_t>& out_user_ids,
                                        std::string* err) {
    out_user_ids.clear();
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }

    const char* sql =
        "SELECT user_id FROM im_talk_session WHERE talk_id = ? AND last_msg_id = ? AND deleted_at IS NULL";
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
}  // namespace CIM::dao