#include "dao/talk_dao.hpp"

#include <algorithm>

namespace CIM::dao {

static constexpr const char* kDBName = "default";

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

bool TalkDao::findOrCreateSingleTalk(const std::shared_ptr<CIM::MySQL>& db, uint64_t uid1,
                                     uint64_t uid2, uint64_t& out_talk_id, std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    uint64_t umin = 0, umax = 0;
    order_pair(uid1, uid2, umin, umax);
    const char* sql =
        "INSERT INTO im_talk (talk_mode, user_min_id, user_max_id, created_at, updated_at) "
        "VALUES (1, ?, ?, NOW(), NOW()) "
        "ON DUPLICATE KEY UPDATE updated_at=VALUES(updated_at)";
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

bool TalkDao::findOrCreateGroupTalk(const std::shared_ptr<CIM::MySQL>& db, uint64_t group_id,
                                    uint64_t& out_talk_id, std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "INSERT INTO im_talk (talk_mode, group_id, created_at, updated_at) "
        "VALUES (2, ?, NOW(), NOW()) "
        "ON DUPLICATE KEY UPDATE updated_at=VALUES(updated_at)";
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
    return true;
}

bool TalkDao::getSingleTalkId(const uint64_t uid1, const uint64_t uid2, uint64_t& out_talk_id,
                              std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
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

bool TalkDao::getSingleTalkId(const std::shared_ptr<CIM::MySQL>& db, const uint64_t uid1,
                              const uint64_t uid2, uint64_t& out_talk_id, std::string* err) {
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

bool TalkDao::getGroupTalkId(const uint64_t group_id, uint64_t& out_talk_id, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
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

bool TalkDao::getGroupTalkId(const std::shared_ptr<CIM::MySQL>& db, const uint64_t group_id,
                             uint64_t& out_talk_id, std::string* err) {
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

}  // namespace CIM::dao
