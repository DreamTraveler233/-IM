#include "dao/contact_apply_dao.hpp"

#include "db/mysql.hpp"
#include "util/util.hpp"

namespace CIM::dao {

static constexpr const char* kDBName = "default";

bool ContactApplyDAO::AgreeApplyWithConn(const std::shared_ptr<CIM::MySQL>& db,
                                         const uint64_t user_id, const uint64_t apply_id,
                                         const std::string& remark, std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "UPDATE im_contact_apply SET status = 2, handler_user_id = ?, handle_remark = "
        "?, handled_at = NOW(), updated_at = NOW() WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, user_id);
    stmt->bindString(2, remark);
    stmt->bindUint64(3, apply_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool ContactApplyDAO::GetDetailByIdWithConn(const std::shared_ptr<CIM::MySQL>& db,
                                            const uint64_t apply_id, ContactApply& out,
                                            std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT id, apply_user_id, target_user_id, remark, status, handler_user_id, "
        "handle_remark, handled_at, created_at, updated_at FROM im_contact_apply WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, apply_id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }

    if (!res->next()) {
        if (err) *err = "no record found";
        return false;
    }

    out.id = res->getUint64(0);
    out.apply_user_id = res->getUint64(1);
    out.target_user_id = res->getUint64(2);
    out.remark = res->isNull(3) ? std::string() : res->getString(3);
    out.status = res->getUint8(4);
    out.handler_user_id = res->getUint64(5);
    out.handle_remark = res->isNull(6) ? std::string() : res->getString(6);
    out.handled_at = res->isNull(7) ? 0 : res->getTime(7);
    out.created_at = res->isNull(8) ? 0 : res->getTime(8);
    out.updated_at = res->isNull(9) ? 0 : res->getTime(9);

    return true;
}

bool ContactApplyDAO::GetDetailById(const uint64_t apply_id, ContactApply& out,
                                    std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    return GetDetailByIdWithConn(db, apply_id, out, err);
}

bool ContactApplyDAO::Create(const ContactApply& a, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }

    // 1) 快速检查：是否已有待处理申请（status = 1）
    const char* check_sql =
        "SELECT id FROM im_contact_apply WHERE apply_user_id = ? AND target_user_id = ? AND status "
        "= 1 LIMIT 1";
    auto check_stmt = db->prepare(check_sql);
    if (!check_stmt) {
        if (err) *err = "prepare statement failed";
        return false;
    }
    check_stmt->bindUint64(1, a.apply_user_id);
    check_stmt->bindUint64(2, a.target_user_id);
    auto check_res = check_stmt->query();
    if (!check_res) {
        if (err) *err = "check query failed";
        return false;
    }
    if (check_res->next()) {
        // 已存在待处理记录：根据业务决定，这里返回一个友好错误
        if (err) *err = "pending application already exists";
        return false;
    }

    // 2) 尝试插入
    const char* sql =
        "INSERT INTO im_contact_apply (apply_user_id, target_user_id, remark, status, "
        "handler_user_id, handle_remark, handled_at, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, NOW(), NOW())";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, a.apply_user_id);
    stmt->bindUint64(2, a.target_user_id);
    if (!a.remark.empty())
        stmt->bindString(3, a.remark);
    else
        stmt->bindNull(3);
    stmt->bindUint8(4, a.status);
    if (a.handler_user_id != 0)
        stmt->bindUint64(5, a.handler_user_id);
    else
        stmt->bindNull(5);
    if (!a.handle_remark.empty())
        stmt->bindString(6, a.handle_remark);
    else
        stmt->bindNull(6);
    if (a.handled_at != 0)
        stmt->bindTime(7, a.handled_at);
    else
        stmt->bindNull(7);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool ContactApplyDAO::RejectApply(const uint64_t handler_user_id, const uint64_t apply_user_id,
                                  const std::string& remark, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "UPDATE im_contact_apply SET status = 3, handler_user_id = ?, handle_remark = "
        "?, handled_at = NOW(), updated_at = NOW() WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, handler_user_id);
    stmt->bindString(2, remark);
    stmt->bindUint64(3, apply_user_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool ContactApplyDAO::GetItemById(const uint64_t id, std::vector<ContactApplyItem>& out,
                                  std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT ca.id, ca.target_user_id, ca.apply_user_id, ca.remark, u.nickname, u.avatar, "
        "DATE_FORMAT(ca.created_at, '%Y-%m-%d %H:%i:%s') FROM im_contact_apply ca "
        "LEFT JOIN im_user u ON ca.apply_user_id = u.id "
        "WHERE ca.target_user_id = ? AND ca.status = 1 ";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = stmt->getErrStr();
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
        item.apply_user_id = res->getUint64(1);
        item.target_user_id = res->getUint64(2);
        item.remark = res->isNull(3) ? "" : res->getString(3);
        item.nickname = res->isNull(4) ? "" : res->getString(4);
        item.avatar = res->isNull(5) ? "" : res->getString(5);
        item.created_at = res->isNull(6) ? "" : res->getString(6);
        out.emplace_back(std::move(item));
    }
    return true;
}

bool ContactApplyDAO::GetPendingCountById(uint64_t id, uint64_t& out_count, std::string* err) {
    out_count = 0;
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT COUNT(*) FROM im_contact_apply WHERE target_user_id = ? AND status = 1";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, id);
    auto res = stmt->query();

    if (!res) {
        if (err) *err = "query failed";
        return false;
    }

    if (!res->next()) {
        if (err) *err = "no record found";
        return false;
    }

    out_count = res->getUint64(0);
    return true;
}

}  // namespace CIM::dao
