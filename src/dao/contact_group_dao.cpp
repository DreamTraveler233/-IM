#include "dao/contact_group_dao.hpp"

#include "db/mysql.hpp"

namespace CIM::dao {

static const char* kDBName = "default";

bool ContactGroupDAO::Create(const ContactGroup& g, uint64_t& out_id, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql =
        "INSERT INTO contact_groups (user_id, name, sort, contact_count) VALUES (?, ?, ?, ?)";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindUint64(1, g.user_id);
    stmt->bindString(2, g.name);
    stmt->bindUint32(3, g.sort);
    stmt->bindUint32(4, g.contact_count);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    out_id = static_cast<uint64_t>(stmt->getLastInsertId());
    return true;
}

bool ContactGroupDAO::GetById(uint64_t id, ContactGroup& out) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) return false;
    const char* sql =
        "SELECT id, user_id, name, sort, contact_count, created_at, updated_at FROM contact_groups "
        "WHERE id = ? LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) return false;
    stmt->bindUint64(1, id);
    auto res = stmt->query();
    if (!res || !res->next()) return false;
    out.id = res->getUint64(0);
    out.user_id = res->getUint64(1);
    out.name = res->getString(2);
    out.sort = res->getUint32(3);
    out.contact_count = res->getUint32(4);
    out.created_at = res->getTime(5);
    out.updated_at = res->getTime(6);
    return true;
}

bool ContactGroupDAO::ListByUser(uint64_t user_id, std::vector<ContactGroup>& outs) {
    outs.clear();
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) return false;
    const char* sql =
        "SELECT id, user_id, name, sort, contact_count, created_at, updated_at FROM contact_groups "
        "WHERE user_id = ? ORDER BY sort ASC, id ASC";
    auto stmt = db->prepare(sql);
    if (!stmt) return false;
    stmt->bindUint64(1, user_id);
    auto res = stmt->query();
    if (!res) return false;
    while (res->next()) {
        ContactGroup g;
        g.id = res->getUint64(0);
        g.user_id = res->getUint64(1);
        g.name = res->getString(2);
        g.sort = res->getUint32(3);
        g.contact_count = res->getUint32(4);
        g.created_at = res->getTime(5);
        g.updated_at = res->getTime(6);
        outs.push_back(std::move(g));
    }
    return true;
}

bool ContactGroupDAO::Update(uint64_t id, const std::string& name, uint32_t sort,
                             std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql =
        "UPDATE contact_groups SET name = ?, sort = ?, updated_at = NOW() WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindString(1, name);
    stmt->bindUint32(2, sort);
    stmt->bindUint64(3, id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool ContactGroupDAO::Delete(uint64_t id, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql = "DELETE FROM contact_groups WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindUint64(1, id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

}  // namespace CIM::dao
