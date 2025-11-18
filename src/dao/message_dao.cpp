#include "dao/message_dao.hpp"

#include <sstream>

#include "base/macro.hpp"

namespace CIM::dao {

static constexpr const char* kDBName = "default";
static auto g_logger = CIM_LOG_ROOT();

namespace {
// 统一列选择片段
static const char* kSelectCols =
    "id,talk_id,sequence,talk_mode,msg_type,sender_id,receiver_id,group_id,content_text,extra,"
    "quote_msg_id,is_revoked,revoke_by,revoke_time,created_at,updated_at";
}  // namespace

bool MessageDao::Create(const std::shared_ptr<CIM::MySQL>& db, const Message& m, std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }

    const char* sql =
        "INSERT INTO im_message "
        "(id,talk_id,sequence,talk_mode,msg_type,sender_id,receiver_id,group_id,"  // 9
        "content_text,extra,quote_msg_id,is_revoked,revoke_by,revoke_time,created_at,updated_at) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,NOW(),NOW())";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindString(1, m.id);
    stmt->bindUint64(2, m.talk_id);
    stmt->bindUint64(3, m.sequence);
    stmt->bindInt8(4, m.talk_mode);
    stmt->bindInt16(5, m.msg_type);
    stmt->bindUint64(6, m.sender_id);
    if (m.receiver_id)
        stmt->bindUint64(7, m.receiver_id);
    else
        stmt->bindNull(7);
    if (m.group_id)
        stmt->bindUint64(8, m.group_id);
    else
        stmt->bindNull(8);
    if (!m.content_text.empty())
        stmt->bindString(9, m.content_text);
    else
        stmt->bindNull(9);
    if (!m.extra.empty())
        stmt->bindString(10, m.extra);
    else
        stmt->bindNull(10);
    if (!m.quote_msg_id.empty())
        stmt->bindString(11, m.quote_msg_id);
    else
        stmt->bindNull(11);
    stmt->bindInt8(12, m.is_revoked);
    if (m.revoke_by)
        stmt->bindUint64(13, m.revoke_by);
    else
        stmt->bindNull(13);
    if (m.revoke_time)
        stmt->bindTime(14, m.revoke_time);
    else
        stmt->bindNull(14);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool MessageDao::GetById(const std::string& msg_id, Message& out, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }

    std::ostringstream oss;
    oss << "SELECT " << kSelectCols << " FROM im_message WHERE id=? LIMIT 1";

    auto stmt = db->prepare(oss.str().c_str());
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }

    stmt->bindString(1, msg_id);

    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }

    if (!res->next()) {
        if (err) *err = "no record found";
        return false;
    }

    out.id = res->getString(0);
    out.talk_id = res->getUint64(1);
    out.sequence = res->getUint64(2);
    out.talk_mode = res->getUint8(3);
    out.msg_type = res->getUint16(4);
    out.sender_id = res->getUint64(5);
    out.receiver_id = res->isNull(6) ? 0 : res->getUint64(6);
    out.group_id = res->isNull(7) ? 0 : res->getUint64(7);
    out.content_text = res->isNull(8) ? std::string() : res->getString(8);
    out.extra = res->isNull(9) ? std::string() : res->getString(9);
    out.quote_msg_id = res->isNull(10) ? std::string() : res->getString(10);
    out.is_revoked = res->getUint8(11);
    out.revoke_by = res->isNull(12) ? 0 : res->getUint64(12);
    out.revoke_time = res->isNull(13) ? 0 : res->getTime(13);
    out.created_at = res->getTime(14);
    out.updated_at = res->getTime(15);

    return true;
}

bool MessageDao::ListRecentDesc(const uint64_t talk_id, const uint64_t anchor_seq,
                                const size_t limit, std::vector<Message>& out, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    std::ostringstream oss;
    oss << "SELECT id, talk_id, sequence, talk_mode, msg_type, sender_id, receiver_id, group_id, "
           "content_text, extra, quote_msg_id, is_revoked, revoke_by, revoke_time, created_at, "
           "updated_at FROM im_message WHERE talk_id=?";
    // cursor 表示“从比它更早的消息开始翻页”
    if (anchor_seq > 0) {
        oss << " AND sequence<?";
    }
    oss << " ORDER BY sequence DESC LIMIT ?";
    auto stmt = db->prepare(oss.str().c_str());
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }

    stmt->bindUint64(1, talk_id);
    if (anchor_seq > 0) {
        // SQL 形如: WHERE talk_id=? AND sequence<? ORDER BY ... LIMIT ?
        // 占位符依次为: 1=talk_id, 2=anchor_seq, 3=limit
        stmt->bindUint64(2, anchor_seq);
        stmt->bindInt32(3, static_cast<int32_t>(limit));
    } else {
        // SQL 形如: WHERE talk_id=? ORDER BY ... LIMIT ?
        // 占位符依次为: 1=talk_id, 2=limit
        stmt->bindInt32(2, static_cast<int32_t>(limit));
    }
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }

    while (res->next()) {
        Message m;
        m.id = res->getString(0);
        m.talk_id = res->getUint64(1);
        m.sequence = res->getUint64(2);
        m.talk_mode = res->getUint8(3);
        m.msg_type = res->getUint16(4);
        m.sender_id = res->getUint64(5);
        m.receiver_id = res->isNull(6) ? 0 : res->getUint64(6);
        m.group_id = res->isNull(7) ? 0 : res->getUint64(7);
        m.content_text = res->isNull(8) ? std::string() : res->getString(8);
        m.extra = res->isNull(9) ? std::string() : res->getString(9);
        m.quote_msg_id = res->isNull(10) ? std::string() : res->getString(10);
        m.is_revoked = res->getUint8(11);
        m.revoke_by = res->isNull(12) ? 0 : res->getUint64(12);
        m.revoke_time = res->isNull(13) ? 0 : res->getTime(13);
        m.created_at = res->getTime(14);
        m.updated_at = res->getTime(15);
        out.push_back(std::move(m));
    }

    return true;
}

bool MessageDao::ListRecentDescWithFilter(const uint64_t talk_id, const uint64_t anchor_seq,
                                          const size_t limit, const uint64_t user_id,
                                          const uint16_t msg_type, std::vector<Message>& out,
                                          std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    std::ostringstream oss;
    oss << "SELECT " << kSelectCols << " FROM im_message m WHERE talk_id=?";
    if (anchor_seq > 0) oss << " AND sequence<?";
    if (msg_type != 0) oss << " AND msg_type=?";
    // 过滤掉已撤回的消息（is_revoked != 2 表示已被撤回/不可见）
    oss << " AND m.is_revoked=2";
    // 过滤掉用户已删除的消息
    if (user_id != 0) {
        oss << " AND NOT EXISTS(SELECT 1 FROM im_message_user_delete d WHERE d.msg_id=m.id AND "
               "d.user_id=?)";
    }
    oss << " ORDER BY sequence DESC LIMIT ?";

    auto stmt = db->prepare(oss.str().c_str());
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }

    int idx = 1;
    stmt->bindUint64(idx++, talk_id);
    if (anchor_seq > 0) stmt->bindUint64(idx++, anchor_seq);
    if (msg_type != 0) stmt->bindInt16(idx++, static_cast<int16_t>(msg_type));
    if (user_id != 0) stmt->bindUint64(idx++, user_id);
    stmt->bindInt32(idx++, static_cast<int32_t>(limit));

    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }

    out.clear();
    while (res->next()) {
        Message m;
        m.id = res->getString(0);
        m.talk_id = res->getUint64(1);
        m.sequence = res->getUint64(2);
        m.talk_mode = res->getUint8(3);
        m.msg_type = res->getUint16(4);
        m.sender_id = res->getUint64(5);
        m.receiver_id = res->isNull(6) ? 0 : res->getUint64(6);
        m.group_id = res->isNull(7) ? 0 : res->getUint64(7);
        m.content_text = res->isNull(8) ? std::string() : res->getString(8);
        m.extra = res->isNull(9) ? std::string() : res->getString(9);
        m.quote_msg_id = res->isNull(10) ? std::string() : res->getString(10);
        m.is_revoked = res->getUint8(11);
        m.revoke_by = res->isNull(12) ? 0 : res->getUint64(12);
        m.revoke_time = res->isNull(13) ? 0 : res->getTime(13);
        m.created_at = res->getTime(14);
        m.updated_at = res->getTime(15);
        out.push_back(std::move(m));
    }
    return true;
}

bool MessageDao::GetByIds(const std::vector<std::string>& ids, std::vector<Message>& out,
                          std::string* err) {
    if (ids.empty()) {
        out.clear();
        return true;
    }
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    // chunk ids to avoid huge IN lists
    std::vector<std::string> ids2 = ids;
    std::sort(ids2.begin(), ids2.end());
    ids2.erase(std::unique(ids2.begin(), ids2.end()), ids2.end());
    const size_t CHUNK = 128;
    for (size_t s = 0; s < ids2.size(); s += CHUNK) {
        size_t e = std::min(s + CHUNK, ids2.size());
        std::ostringstream oss;
        oss << "SELECT " << kSelectCols << " FROM im_message WHERE id IN (";
        for (size_t i = s; i < e; ++i) {
            if (i > s) oss << ",";
            oss << "?";
        }
        oss << ")";
        auto stmt = db->prepare(oss.str().c_str());
        if (!stmt) {
            if (err) *err = "prepare sql failed";
            return false;
        }
        for (size_t i = s; i < e; ++i) {
            stmt->bindString(static_cast<int>(i - s + 1), ids2[i]);
        }
        auto res = stmt->query();
        if (!res) {
            if (err) *err = "query failed";
            return false;
        }
        out.clear();
        while (res->next()) {
            Message m;
            m.id = res->getString(0);
            m.talk_id = res->getUint64(1);
            m.sequence = res->getUint64(2);
            m.talk_mode = res->getUint8(3);
            m.msg_type = res->getUint16(4);
            m.sender_id = res->getUint64(5);
            m.receiver_id = res->isNull(6) ? 0 : res->getUint64(6);
            m.group_id = res->isNull(7) ? 0 : res->getUint64(7);
            m.content_text = res->isNull(8) ? std::string() : res->getString(8);
            m.extra = res->isNull(9) ? std::string() : res->getString(9);
            m.quote_msg_id = res->isNull(10) ? std::string() : res->getString(10);
            m.is_revoked = res->getUint8(11);
            m.revoke_by = res->isNull(12) ? 0 : res->getUint64(12);
            m.revoke_time = res->isNull(13) ? 0 : res->getTime(13);
            m.created_at = res->getTime(14);
            m.updated_at = res->getTime(15);
            out.push_back(std::move(m));
        }
    }
    return true;
}

bool MessageDao::GetByIdsWithFilter(const std::vector<std::string>& ids, const uint64_t user_id,
                                    std::vector<Message>& out, std::string* err) {
    if (ids.empty()) {
        out.clear();
        return true;
    }
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    std::ostringstream oss;
    oss << "SELECT " << kSelectCols << " FROM im_message WHERE id IN (";
    // dedup and chunk
    std::vector<std::string> ids2 = ids;
    std::sort(ids2.begin(), ids2.end());
    ids2.erase(std::unique(ids2.begin(), ids2.end()), ids2.end());
    const size_t CHUNK = 128;
    for (size_t s = 0; s < ids2.size(); s += CHUNK) {
        size_t e = std::min(s + CHUNK, ids2.size());
        std::ostringstream oss;
        oss << "SELECT " << kSelectCols << " FROM im_message WHERE id IN (";
        for (size_t i = s; i < e; ++i) {
            if (i > s) oss << ",";
            oss << "?";
        }
        oss << ")";
        if (user_id != 0) {
            oss << " AND NOT EXISTS(SELECT 1 FROM im_message_user_delete d WHERE "
                   "d.msg_id=im_message.id AND d.user_id=? )";
        }
        auto stmt = db->prepare(oss.str().c_str());
        if (!stmt) {
            if (err) *err = "prepare sql failed";
            return false;
        }
        int idx = 1;
        for (size_t i = s; i < e; ++i) {
            stmt->bindString(idx++, ids2[i]);
        }
        if (user_id != 0) stmt->bindUint64(idx++, user_id);
        auto res = stmt->query();
        if (!res) {
            if (err) *err = "query failed";
            return false;
        }
        out.clear();
        while (res->next()) {
            Message m;
            m.id = res->getString(0);
            m.talk_id = res->getUint64(1);
            m.sequence = res->getUint64(2);
            m.talk_mode = res->getUint8(3);
            m.msg_type = res->getUint16(4);
            m.sender_id = res->getUint64(5);
            m.receiver_id = res->isNull(6) ? 0 : res->getUint64(6);
            m.group_id = res->isNull(7) ? 0 : res->getUint64(7);
            m.content_text = res->isNull(8) ? std::string() : res->getString(8);
            m.extra = res->isNull(9) ? std::string() : res->getString(9);
            m.quote_msg_id = res->isNull(10) ? std::string() : res->getString(10);
            m.is_revoked = res->getUint8(11);
            m.revoke_by = res->isNull(12) ? 0 : res->getUint64(12);
            m.revoke_time = res->isNull(13) ? 0 : res->getTime(13);
            m.created_at = res->getTime(14);
            m.updated_at = res->getTime(15);
            out.push_back(std::move(m));
        }
    }
    return true;
}

bool MessageDao::ListAfterAsc(const uint64_t talk_id, const uint64_t after_seq, const size_t limit,
                              std::vector<Message>& out, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    std::ostringstream oss;
    oss << "SELECT " << kSelectCols
        << " FROM im_message WHERE talk_id=? AND sequence>? ORDER BY sequence ASC LIMIT ?";
    auto stmt = db->prepare(oss.str().c_str());
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, talk_id);
    stmt->bindUint64(2, after_seq);
    stmt->bindInt32(3, static_cast<int32_t>(limit));
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    out.clear();
    while (res->next()) {
        Message m;
        m.id = res->getString(0);
        m.talk_id = res->getUint64(1);
        m.sequence = res->getUint64(2);
        m.talk_mode = static_cast<uint8_t>(res->getInt8(3));
        m.msg_type = static_cast<uint16_t>(res->getInt16(4));
        m.sender_id = res->getUint64(5);
        m.receiver_id = res->isNull(6) ? 0 : res->getUint64(6);
        m.group_id = res->isNull(7) ? 0 : res->getUint64(7);
        m.content_text = res->isNull(8) ? std::string() : res->getString(8);
        m.extra = res->isNull(9) ? std::string() : res->getString(9);
        m.quote_msg_id = res->isNull(10) ? std::string() : res->getString(10);
        m.is_revoked = static_cast<uint8_t>(res->getInt8(11));
        m.revoke_by = res->isNull(12) ? 0 : res->getUint64(12);
        m.revoke_time = res->isNull(13) ? 0 : res->getTime(13);
        m.created_at = res->getTime(14);
        m.updated_at = res->getTime(15);
        out.push_back(std::move(m));
    }
    return true;
}

bool MessageDao::Revoke(const std::string& msg_id, const uint64_t user_id, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "UPDATE im_message SET is_revoked=1, revoke_by=?, revoke_time=NOW(), updated_at=NOW() "
        "WHERE id=? AND is_revoked=2";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, user_id);
    stmt->bindString(2, msg_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;  // 不强制校验影响行数；由上层根据业务判断是否成功撤回
}

}  // namespace CIM::dao
