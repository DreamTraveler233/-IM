#include "dao/contact_dao.hpp"

#include "db/mysql.hpp"

namespace CIM::dao {

static constexpr const char* kDBName = "default";

bool ContactDAO::ListByUser(uint64_t user_id, std::vector<ContactItem>& out, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT c.group_id, c.remark, u.id AS user_id, u.nickname, u.gender, u.motto, u.avatar "
        "FROM im_contact c JOIN im_user u ON c.friend_user_id = u.id "
        "WHERE c.owner_user_id = ? AND c.status = 1 "
        "ORDER BY c.created_at";
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
        ContactItem item;
        item.group_id = res->isNull(0) ? 0 : res->getUint64(0);
        item.remark = res->isNull(1) ? std::string() : res->getString(1);
        item.user_id = res->getUint64(2);
        item.nickname = res->isNull(3) ? std::string() : res->getString(3);
        item.gender = res->isNull(4) ? 0 : res->getUint8(4);
        item.motto = res->isNull(5) ? std::string() : res->getString(5);
        item.avatar = res->isNull(6) ? std::string() : res->getString(6);
        out.emplace_back(std::move(item));
    }

    return true;
}

bool ContactDAO::GetByOwnerAndTarget(uint64_t owner_id, uint64_t target_id, ContactDetails& out,
                                     std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT u.id AS user_id, u.avatar, u.gender, u.mobile, u.motto, u.nickname, u.email,"
        "c.relation, c.group_id AS contact_group_id, c.remark AS contact_remark FROM im_user u "
        "LEFT JOIN im_contact c ON u.id = c.friend_user_id AND c.owner_user_id = ? WHERE u.id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, owner_id);
    stmt->bindUint64(2, target_id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }

    if (!res->next()) {
        if (err) *err = "no record found";
        return false;
    }

    out.user_id = res->getUint64(0);
    out.avatar = res->isNull(1) ? std::string() : res->getString(1);
    out.gender = res->isNull(2) ? 0 : res->getUint8(2);
    out.mobile = res->isNull(3) ? std::string() : res->getString(3);
    out.motto = res->isNull(4) ? std::string() : res->getString(4);
    out.nickname = res->isNull(5) ? std::string() : res->getString(5);
    out.email = res->isNull(6) ? std::string() : res->getString(6);
    out.relation = res->isNull(7) ? 1 : res->getUint8(7);
    out.contact_group_id = res->isNull(8) ? 0 : res->getUint32(8);
    out.contact_remark = res->isNull(9) ? std::string() : res->getString(9);

    return true;
}

bool ContactDAO::GetByOwnerAndTargetWithConn(const std::shared_ptr<CIM::MySQL>& db,
                                             const uint64_t owner_id, const uint64_t target_id,
                                             ContactDetails& out, std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT u.id AS user_id, u.avatar, u.gender, u.mobile, u.motto, u.nickname, u.email,"
        "c.relation, c.group_id AS contact_group_id, c.remark AS contact_remark FROM im_user u "
        "LEFT JOIN im_contact c ON u.id = c.friend_user_id AND c.owner_user_id = ? WHERE u.id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, owner_id);
    stmt->bindUint64(2, target_id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }

    if (!res->next()) {
        if (err) *err = "no record found";
        return false;
    }

    out.user_id = res->getUint64(0);
    out.avatar = res->isNull(1) ? std::string() : res->getString(1);
    out.gender = res->isNull(2) ? 0 : res->getUint8(2);
    out.mobile = res->isNull(3) ? std::string() : res->getString(3);
    out.motto = res->isNull(4) ? std::string() : res->getString(4);
    out.nickname = res->isNull(5) ? std::string() : res->getString(5);
    out.email = res->isNull(6) ? std::string() : res->getString(6);
    out.relation = res->isNull(7) ? 1 : res->getUint8(7);
    out.contact_group_id = res->isNull(8) ? 0 : res->getUint32(8);
    out.contact_remark = res->isNull(9) ? std::string() : res->getString(9);

    return true;
}
bool ContactDAO::UpsertWithConn(const std::shared_ptr<CIM::MySQL>& db, const Contact& c,
                                std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    // 使用 ON DUPLICATE KEY 实现有则更新、无则插入的逻辑
    const char* sql =
        "INSERT INTO im_contact (owner_user_id, friend_user_id, group_id, remark, status, relation,"
        "is_block, created_at, updated_at, deleted_at) VALUES (?, ?, ?, ?, ?, ?, ?, NOW(), NOW(), "
        "?) "
        "ON DUPLICATE KEY UPDATE "
        "group_id = VALUES(group_id), "
        "relation = VALUES(relation), "
        "remark = VALUES(remark), "
        "status = VALUES(status), "
        "updated_at = NOW()";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, c.owner_user_id);
    stmt->bindUint64(2, c.friend_user_id);
    if (c.group_id == 0) {
        stmt->bindNull(3);
    } else {
        stmt->bindUint64(3, c.group_id);
    }
    stmt->bindString(4, c.remark);
    stmt->bindUint8(5, c.status);
    stmt->bindUint8(6, c.relation);
    stmt->bindUint8(7, c.is_block);
    if (c.deleted_at == 0) {
        stmt->bindNull(8);
    } else {
        stmt->bindTime(8, c.deleted_at);
    }
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool ContactDAO::EditRemark(const std::shared_ptr<CIM::MySQL>& db, const uint64_t user_id,
                            const uint64_t contact_id, const std::string& remark,
                            std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "UPDATE im_contact SET remark = ? WHERE owner_user_id = ? AND friend_user_id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindString(1, remark);
    stmt->bindUint64(2, user_id);
    stmt->bindUint64(3, contact_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool ContactDAO::DeleteWithConn(const std::shared_ptr<CIM::MySQL>& db, const uint64_t user_id,
                                const uint64_t contact_id, std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "UPDATE im_contact SET remark = '', relation = 1, status = 2, deleted_at = NOW() WHERE "
        "owner_user_id = ? "
        "AND friend_user_id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, user_id);
    stmt->bindUint64(2, contact_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool ContactDAO::ChangeGroupWithConn(const std::shared_ptr<CIM::MySQL>& db, const uint64_t user_id,
                                     const uint64_t contact_id, const uint64_t group_id,
                                     std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "UPDATE im_contact SET group_id = ? WHERE friend_user_id = ? AND owner_user_id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, group_id);
    stmt->bindUint64(2, contact_id);
    stmt->bindUint64(3, user_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool ContactDAO::GetOldGroupIdWithConn(const std::shared_ptr<CIM::MySQL>& db,
                                       const uint64_t user_id, const uint64_t contact_id,
                                       uint64_t& out_group_id, std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT group_id FROM im_contact WHERE friend_user_id = ? AND owner_user_id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, contact_id);
    stmt->bindUint64(2, user_id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }

    if (!res->next()) {
        if (err) *err = "no record found";
        return false;
    }

    out_group_id = res->isNull(0) ? 0 : res->getUint64(0);

    return true;
}

bool ContactDAO::RemoveFromGroupWithConn(const std::shared_ptr<CIM::MySQL>& db,
                                         const uint64_t user_id, const uint64_t contact_id,
                                         std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "UPDATE im_contact SET group_id = NULL WHERE owner_user_id = ? AND friend_user_id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, user_id);
    stmt->bindUint64(2, contact_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool ContactDAO::RemoveFromGroupByGroupIdWithConn(const std::shared_ptr<CIM::MySQL>& db,
                                                  const uint64_t user_id, const uint64_t group_id,
                                                  std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "UPDATE im_contact SET group_id = NULL WHERE owner_user_id = ? AND group_id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, user_id);
    stmt->bindUint64(2, group_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}
}  // namespace CIM::dao