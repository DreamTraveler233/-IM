#include "dao/contact_apply_dao.hpp"

#include "db/mysql.hpp"
#include "util/util.hpp"

namespace CIM::dao {

static const char* kDBName = "default";

bool ContactApplyDAO::Create(const ContactApply& a, uint64_t& out_id, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql =
        "INSERT INTO contact_applies (applicant_id, target_id, remark, status, "
        "handle_remark, handled_at) VALUES (?, ?, ?, ?, ?, ?)";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindUint64(1, a.applicant_id);
    stmt->bindUint64(2, a.target_id);
    if (!a.remark.empty())
        stmt->bindString(3, a.remark);
    else
        stmt->bindNull(3);
    stmt->bindInt32(4, a.status);
    if (!a.handle_remark.empty())
        stmt->bindString(5, a.handle_remark);
    else
        stmt->bindNull(5);
    if (a.handled_at != 0)
        stmt->bindTime(6, a.handled_at);
    else
        stmt->bindNull(6);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    out_id = static_cast<uint64_t>(stmt->getLastInsertId());
    return true;
}

bool ContactApplyDAO::GetPendingCountById(uint64_t id, uint64_t& out_count, std::string* err) {
    out_count = 0;
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql = "SELECT COUNT(*) FROM contact_applies WHERE target_id = ? AND status = 0";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindUint64(1, id);
    auto res = stmt->query();
    if (!res || !res->next()) {
        if (err) *err = "query failed";
        return false;
    }
    out_count = res->getUint64(0);
    return true;
}

bool ContactApplyDAO::GetItemById(uint64_t id, std::vector<ContactApplyItem>& out,
                                  std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql =
        "SELECT ca.id, ca.target_id, ca.applicant_id, ca.remark, u.nickname, u.avatar, "
        "DATE_FORMAT(ca.created_at, '%Y-%m-%d %H:%i:%s') "
        "FROM contact_applies ca "
        "LEFT JOIN users u ON ca.applicant_id = u.id "
        "WHERE ca.target_id = ? AND ca.status = 0 ";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindUint64(1, id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    while (res->next()) {
        ContactApplyItem item;
        item.id = res->getUint64(0);
        item.user_id = res->getUint64(1);
        item.friend_id = res->getUint64(2);
        item.remark = res->isNull(3) ? std::string() : res->getString(3);
        item.nickname = res->isNull(4) ? std::string() : res->getString(4);
        item.avatar = res->isNull(5) ? std::string() : res->getString(5);
        item.created_at = res->isNull(6) ? std::string() : res->getString(6);
        out.emplace_back(std::move(item));
    }

    return true;
}

bool ContactApplyDAO::AgreeApply(uint64_t apply_id, const std::string& remark, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql =
        "UPDATE contact_applies SET status = 1, handle_remark = ?, handled_at = ? WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindString(1, remark);
    stmt->bindTime(2, CIM::TimeUtil::NowToS());
    stmt->bindUint64(3, apply_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool ContactApplyDAO::RejectApply(uint64_t apply_id, const std::string& remark, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql =
        "UPDATE contact_applies SET status = 2, handle_remark = ?, handled_at = ? WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindString(1, remark);
    stmt->bindTime(2, CIM::TimeUtil::NowToS());
    stmt->bindUint64(3, apply_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool ContactApplyDAO::GetDetailById(uint64_t apply_id, ContactApply& out, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql =
        "SELECT id, applicant_id, target_id, remark, status, handle_remark, "
        "handled_at, created_at, updated_at "
        "FROM contact_applies WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindUint64(1, apply_id);
    auto res = stmt->query();
    if (!res || !res->next()) {
        if (err) *err = "query failed";
        return false;
    }
    out.id = res->getUint64(0);
    out.applicant_id = res->getUint64(1);
    out.target_id = res->getUint64(2);
    out.remark = res->isNull(3) ? std::string() : res->getString(3);
    out.status = res->getUint8(4);
    out.handle_remark = res->isNull(5) ? std::string() : res->getString(5);
    out.handled_at = res->isNull(6) ? 0 : res->getTime(6);
    out.created_at = res->isNull(7) ? 0 : res->getTime(7);
    out.updated_at = res->isNull(8) ? 0 : res->getTime(8);
    return true;
}
}  // namespace CIM::dao
