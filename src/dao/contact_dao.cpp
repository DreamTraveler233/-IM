#include "dao/contact_dao.hpp"

#include "db/mysql.hpp"

namespace CIM::dao {

static const char* kDBName = "default";
bool ContactDAO::ListByUser(uint64_t user_id, std::vector<ContactItem>& out, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "db error";
        return false;
    }
    const char* sql =
        "SELECT c.group_id, c.remark, u.id AS user_id, u.nickname, u.gender, u.motto, u.avatar "
        "FROM contacts c JOIN users u ON c.contact_id = u.id "
        "WHERE c.user_id = ? AND c.status = 1 "
        "ORDER BY c.created_at";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
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
        item.group_id = res->getUint64(0);
        item.remark = res->isNull(1) ? std::string() : res->getString(1);
        item.user_id = res->getUint64(2);
        item.nickname = res->isNull(3) ? std::string() : res->getString(3);
        item.gender = res->isNull(4) ? 0 : res->getUint32(4);
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
        if (err) *err = "db error";
        return false;
    }
    const char* sql =
        "SELECT u.id AS user_id, u.avatar, u.gender, u.mobile, u.motto, u.nickname, u.email,"
        "c.relation, c.group_id AS contact_group_id, c.remark AS contact_remark FROM users u "
        "LEFT JOIN contacts c ON u.id = c.contact_id AND c.user_id = ? WHERE u.id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) return false;
    stmt->bindUint64(1, owner_id);   // c.user_id = owner_id (请求者)
    stmt->bindUint64(2, target_id);  // u.id = target_id (被查看者)
    auto res = stmt->query();
    if (!res) return false;
    if (res->next()) {
        out.user_id = res->getUint64(0);
        out.avatar = res->isNull(1) ? "" : res->getString(1);
        out.gender = res->isNull(2) ? 0 : res->getUint32(2);
        out.mobile = res->isNull(3) ? "" : res->getString(3);
        out.motto = res->isNull(4) ? "" : res->getString(4);
        out.nickname = res->isNull(5) ? "" : res->getString(5);
        out.email = res->isNull(6) ? "" : res->getString(6);
        out.relation = res->isNull(7) ? 1 : res->getUint32(7);
        out.contact_group_id = res->isNull(8) ? 0 : res->getUint32(8);
        out.contact_remark = res->isNull(9) ? "" : res->getString(9);
        return true;
    }
    return false;
}

bool ContactDAO::Create(const Contact& c, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "db error";
        return false;
    }
    const char* sql =
        "INSERT INTO contacts (user_id, contact_id, relation, group_id, remark, created_at, "
        "status) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindUint64(1, c.user_id);
    stmt->bindUint64(2, c.contact_id);
    stmt->bindUint8(3, c.relation);
    if (c.group_id == 0) {
        stmt->bindNull(4);
    } else {
        stmt->bindUint64(4, c.group_id);
    }
    stmt->bindString(5, c.remark);
    stmt->bindTime(6, c.created_at);
    stmt->bindUint8(7, c.status);
    int rc = stmt->execute();
    if (rc != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}
}  // namespace CIM::dao
