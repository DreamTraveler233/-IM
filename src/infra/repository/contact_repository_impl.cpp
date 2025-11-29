#include "infra/repository/contact_repository_impl.hpp"

namespace IM::infra::repository {
static constexpr const char* kDBName = "default";

ContactRepositoryImpl::ContactRepositoryImpl(std::shared_ptr<IM::MySQLManager> db_manager)
    : m_db_manager(std::move(db_manager)) {}

bool ContactRepositoryImpl::GetContactItemListByUserId(uint64_t user_id,
                                                       std::vector<dto::ContactItem>& out,
                                                       std::string* err) {
    auto db = m_db_manager->get(kDBName);
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
        dto::ContactItem item;
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

bool ContactRepositoryImpl::GetByOwnerAndTarget(const uint64_t owner_id, const uint64_t target_id,
                                                dto::ContactDetails& out, std::string* err) {
    auto db = m_db_manager->get(kDBName);
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

bool ContactRepositoryImpl::GetByOwnerAndTarget(const std::shared_ptr<IM::MySQL>& db,
                                                const uint64_t owner_id, const uint64_t target_id,
                                                dto::ContactDetails& out, std::string* err) {
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

bool ContactRepositoryImpl::UpsertContact(const std::shared_ptr<IM::MySQL>& db,
                                          const model::Contact& c, std::string* err) {
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
        "updated_at = NOW(), "
        "deleted_at = VALUES(deleted_at)";
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

bool ContactRepositoryImpl::EditRemark(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
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

bool ContactRepositoryImpl::DeleteContact(const std::shared_ptr<IM::MySQL>& db,
                                          const uint64_t user_id, const uint64_t contact_id,
                                          std::string* err) {
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

bool ContactRepositoryImpl::UpdateStatusAndRelation(const std::shared_ptr<IM::MySQL>& db,
                                                    const uint64_t user_id,
                                                    const uint64_t contact_id, const uint8_t status,
                                                    const uint8_t relation, std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "UPDATE im_contact SET status = ?, relation = ? WHERE friend_user_id = ? AND owner_user_id "
        "= ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint8(1, status);
    stmt->bindUint8(2, relation);
    stmt->bindUint64(3, contact_id);
    stmt->bindUint64(4, user_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool ContactRepositoryImpl::ChangeContactGroup(const std::shared_ptr<IM::MySQL>& db,
                                               const uint64_t user_id, const uint64_t contact_id,
                                               const uint64_t group_id, std::string* err) {
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

bool ContactRepositoryImpl::GetOldGroupId(const std::shared_ptr<IM::MySQL>& db,
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

bool ContactRepositoryImpl::RemoveFromGroup(const std::shared_ptr<IM::MySQL>& db,
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

bool ContactRepositoryImpl::RemoveFromGroupByGroupId(const std::shared_ptr<IM::MySQL>& db,
                                                     const uint64_t user_id,
                                                     const uint64_t group_id, std::string* err) {
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

bool ContactRepositoryImpl::CreateContactGroup(const std::shared_ptr<IM::MySQL>& db,
                                               const model::ContactGroup& g, uint64_t& out_id,
                                               std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "INSERT INTO im_contact_group (user_id, name, sort, contact_count, created_at, updated_at) "
        "VALUES (?, ?, "
        "?, ?, NOW(), NOW())";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
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

bool ContactRepositoryImpl::GetContactGroupItemListByUserId(
    const uint64_t user_id, std::vector<dto::ContactGroupItem>& outs, std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT id, name, contact_count, sort FROM im_contact_group "
        "WHERE user_id = ? ORDER BY sort ASC, id ASC";
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
        dto::ContactGroupItem g;
        g.id = res->getUint64(0);
        g.name = res->isNull(1) ? std::string() : res->getString(1);
        g.contact_count = res->getUint32(2);
        g.sort = res->getUint32(3);
        outs.push_back(std::move(g));
    }
    return true;
}

bool ContactRepositoryImpl::GetContactGroupItemListByUserId(
    const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
    std::vector<dto::ContactGroupItem>& outs, std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT id, name, contact_count, sort FROM im_contact_group "
        "WHERE user_id = ? ORDER BY sort ASC, id ASC";
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
        dto::ContactGroupItem g;
        g.id = res->getUint64(0);
        g.name = res->isNull(1) ? std::string() : res->getString(1);
        g.contact_count = res->getUint32(2);
        g.sort = res->getUint32(3);
        outs.push_back(std::move(g));
    }
    return true;
}

bool ContactRepositoryImpl::UpdateContactGroup(const std::shared_ptr<IM::MySQL>& db,
                                               const uint64_t id, const uint32_t sort,
                                               const std::string& name, std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql = "UPDATE im_contact_group SET name = ?, sort = ? WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
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

bool ContactRepositoryImpl::DeleteContactGroup(const std::shared_ptr<IM::MySQL>& db,
                                               const uint64_t id, std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql = "DELETE FROM im_contact_group WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool ContactRepositoryImpl::UpdateContactCount(const std::shared_ptr<IM::MySQL>& db,
                                               const uint64_t group_id, bool increase,
                                               std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql;
    if (!increase) {
        // 避免无符号整数下溢
        sql =
            "UPDATE im_contact_group SET contact_count = GREATEST(contact_count - 1, 0) WHERE id = "
            "?";
    } else {
        sql = "UPDATE im_contact_group SET contact_count = contact_count + 1 WHERE id = ?";
    }
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

bool ContactRepositoryImpl::AgreeApply(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                                       const uint64_t apply_id, const std::string& remark,
                                       std::string* err) {
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

bool ContactRepositoryImpl::GetDetailById(const std::shared_ptr<IM::MySQL>& db,
                                          const uint64_t apply_id, model::ContactApply& out,
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

bool ContactRepositoryImpl::GetDetailById(const uint64_t apply_id, model::ContactApply& out,
                                          std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    return GetDetailById(db, apply_id, out, err);
}

bool ContactRepositoryImpl::CreateContactApply(const model::ContactApply& a, std::string* err) {
    auto db = m_db_manager->get(kDBName);
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

bool ContactRepositoryImpl::RejectApply(const uint64_t handler_user_id,
                                        const uint64_t apply_user_id, const std::string& remark,
                                        std::string* err) {
    auto db = m_db_manager->get(kDBName);
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

bool ContactRepositoryImpl::GetContactApplyItemById(const uint64_t id,
                                                    std::vector<dto::ContactApplyItem>& out,
                                                    std::string* err) {
    auto db = m_db_manager->get(kDBName);
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
        dto::ContactApplyItem item;
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

bool ContactRepositoryImpl::GetPendingCountById(uint64_t id, uint64_t& out_count,
                                                std::string* err) {
    out_count = 0;
    auto db = m_db_manager->get(kDBName);
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

}  // namespace IM::infra::repository