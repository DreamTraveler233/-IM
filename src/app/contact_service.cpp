#include "app/contact_service.hpp"

#include <unordered_set>
#include <vector>

#include "dao/contact_apply_dao.hpp"
#include "dao/contact_dao.hpp"
#include "dao/user_dao.hpp"
#include "db/mysql.hpp"
#include "base/macro.hpp"

namespace CIM::app {
static auto g_logger = CIM_LOG_NAME("root");
static constexpr const char* kDBName = "default";

VoidResult ContactService::AgreeApply(const uint64_t user_id, const uint64_t apply_id,
                                      const std::string& remark) {
    VoidResult result;
    std::string err;

    // 1. 开启数据库事务，保证后续操作的原子性
    auto trans = CIM::MySQLMgr::GetInstance()->openTransaction(kDBName, false);
    if (!trans) {
        CIM_LOG_ERROR(g_logger) << "AgreeApply openTransaction failed, apply_id=" << apply_id;
        result.code = 500;
        result.err = "处理好友申请失败";
        return result;
    }

    // 2. 获取事务绑定的数据库连接
    auto db = trans->getMySQL();
    if (!db) {
        CIM_LOG_ERROR(g_logger) << "AgreeApply get transaction connection failed, apply_id="
                                << apply_id;
        result.code = 500;
        result.err = "处理好友申请失败";
        return result;
    }

    // 3. 更新申请状态为已同意
    if (!CIM::dao::ContactApplyDAO::AgreeApplyWithConn(db, user_id, apply_id, remark, &err)) {
        trans->rollback();
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger)
                << "HandleContactApply AgreeApply failed, apply_id=" << apply_id << ", err=" << err;
            result.code = 500;
            result.err = "更新好友申请状态失败";
            return result;
        }
    }

    // 4. 获取申请详情
    CIM::dao::ContactApply apply;
    if (!CIM::dao::ContactApplyDAO::GetDetailByIdWithConn(db, apply_id, apply, &err)) {
        trans->rollback();
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger)
                << "HandleContactApply GetDetailById failed, apply_id=" << apply_id
                << ", err=" << err;
            result.code = 500;
            result.err = "获取好友申请详情失败";
            return result;
        }
    }

    // 5. 使用 SQL 级别的 upsert（插入或更新）简化：无记录则创建，有记录则把 status/relation 恢复为好友状态
    //    第一次 upsert：目标用户添加申请人
    CIM::dao::Contact c1;
    c1.owner_user_id = apply.target_user_id;
    c1.friend_user_id = apply.apply_user_id;
    c1.group_id = 0;  // 默认分组
    c1.status = 1;    // 正常
    c1.relation = 2;  // 好友
    if (!CIM::dao::ContactDAO::UpsertWithConn(db, c1, &err)) {
        trans->rollback();
        if (!err.empty()) {
            // 失败则回滚事务，防止只建立单向好友关系
            CIM_LOG_ERROR(g_logger)
                << "AgreeApply Upsert failed for applicant->target, apply_id=" << apply_id
                << ", err=" << err;
            result.code = 500;
            result.err = "创建/更新好友记录失败";
            return result;
        }
    }

    CIM::dao::Contact c2;
    // 第二次 upsert：申请人添加目标用户
    c2.owner_user_id = apply.apply_user_id;
    c2.friend_user_id = apply.target_user_id;
    c2.group_id = 0;
    c2.status = 1;
    c2.relation = 2;
    if (!CIM::dao::ContactDAO::UpsertWithConn(db, c2, &err)) {
        trans->rollback();
        if (!err.empty()) {
            // 失败则回滚事务，防止只建立单向好友关系
            CIM_LOG_ERROR(g_logger)
                << "AgreeApply Upsert failed for applicant->target, apply_id=" << apply_id
                << ", err=" << err;
            result.code = 500;
            result.err = "创建/更新好友记录失败";
            return result;
        }
    }

    // 6. 提交事务，只有全部成功才真正写入数据库
    if (!trans->commit()) {
        // 提交失败也要回滚，保证数据一致性
        const auto commit_err = db->getErrStr();
        trans->rollback();
        CIM_LOG_ERROR(g_logger) << "AgreeApply commit transaction failed, apply_id=" << apply_id
                                << ", err=" << commit_err;
        result.code = 500;
        result.err = "处理好友申请失败";
        return result;
    }

    result.ok = true;
    return result;
}

VoidResult ContactService::CreateContactApply(uint64_t apply_user_id, uint64_t target_user_id,
                                              const std::string& remark) {
    VoidResult result;
    std::string err;

    CIM::dao::ContactApply apply;
    apply.apply_user_id = apply_user_id;
    apply.target_user_id = target_user_id;
    apply.remark = remark;
    if (!CIM::dao::ContactApplyDAO::Create(apply, &err)) {
        if (!err.empty() && err != "pending application already exists") {
            CIM_LOG_ERROR(g_logger) << "CreateContactApply failed, apply_user_id=" << apply_user_id
                                    << ", target_user_id=" << target_user_id << ", err=" << err;
            result.code = 500;
            result.err = "创建好友申请失败";
            return result;
        }
    }

    result.ok = true;
    return result;
}

VoidResult ContactService::RejectApply(const uint64_t handler_user_id, const uint64_t apply_user_id,
                                       const std::string& remark) {
    VoidResult result;
    std::string err;

    if (!CIM::dao::ContactApplyDAO::RejectApply(handler_user_id, apply_user_id, remark, &err)) {
        CIM_LOG_ERROR(g_logger) << "HandleContactApply RejectApply failed, apply_user_id="
                                << apply_user_id << ", err=" << err;
        result.code = 500;
        result.err = "处理好友申请失败";
        return result;
    }

    result.ok = true;
    return result;
}

ContactApplyListResult ContactService::ListContactApplies(uint64_t user_id) {
    ContactApplyListResult result;
    std::string err;

    if (!CIM::dao::ContactApplyDAO::GetItemById(user_id, result.data, &err)) {
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger)
                << "ListContactApplies failed, user_id=" << user_id << ", err=" << err;
            result.code = 500;
            result.err = "获取好友申请列表失败";
            return result;
        }
    }
    result.ok = true;
    return result;
}

ContactGroupListResult ContactService::GetContactGroupLists(const uint64_t user_id) {
    ContactGroupListResult result;
    std::string err;

    if (!CIM::dao::ContactGroupDAO::ListByUserId(user_id, result.data, &err)) {
        CIM_LOG_ERROR(g_logger) << "ListContactGroups failed, user_id=" << user_id
                                << ", err=" << err;
        result.code = 500;
        result.err = "获取联系人分组列表失败";
        return result;
    }
    result.ok = true;
    return result;
}

UserResult ContactService::SearchByMobile(const std::string& mobile) {
    UserResult result;
    std::string err;

    if (!CIM::dao::UserDAO::GetByMobile(mobile, result.data, &err)) {
        CIM_LOG_ERROR(g_logger) << "SearchByMobile failed, mobile=" << mobile << ", err=" << err;
        result.code = 404;
        result.err = "联系人不存在";
        return result;
    }
    result.ok = true;
    return result;
}

ContactDetailsResult ContactService::GetContactDetail(const uint64_t owner_id,
                                                      const uint64_t target_id) {
    ContactDetailsResult result;
    std::string err;

    if (!CIM::dao::ContactDAO::GetByOwnerAndTarget(owner_id, target_id, result.data, &err)) {
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger) << "GetContactDetail failed, owner_id=" << owner_id
                                    << ", target_id=" << target_id << ", err=" << err;
            result.code = 500;
            result.err = "获取联系人详情失败";
            return result;
        }
    }
    result.ok = true;
    return result;
}

ContactListResult ContactService::ListFriends(const uint64_t user_id) {
    ContactListResult result;
    std::string err;

    if (!CIM::dao::ContactDAO::ListByUser(user_id, result.data, &err)) {
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger) << "ListFriends failed, user_id=" << user_id << ", err=" << err;
            result.code = 500;
            result.err = "获取好友列表失败";
            return result;
        }
    }
    result.ok = true;
    return result;
}

ApplyCountResult ContactService::GetPendingContactApplyCount(uint64_t user_id) {
    ApplyCountResult result;
    std::string err;

    if (!CIM::dao::ContactApplyDAO::GetPendingCountById(user_id, result.data, &err)) {
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger)
                << "GetPendingContactApplyCount failed, user_id=" << user_id << ", err=" << err;
            result.code = 500;
            result.err = "获取未处理的好友申请数量失败";
            return result;
        }
    }

    result.ok = true;
    return result;
}

VoidResult ContactService::EditContactRemark(const uint64_t user_id, const uint64_t contact_id,
                                             const std::string& remark) {
    VoidResult result;
    std::string err;

    // 1. 启动事务
    auto trans = CIM::MySQLMgr::GetInstance()->openTransaction(kDBName, false);
    if (!trans) {
        CIM_LOG_ERROR(g_logger) << "EditContactRemark openTransaction failed, user_id=" << user_id
                                << ", contact_id=" << contact_id;
        result.code = 500;
        result.err = "修改联系人备注失败";
        return result;
    }

    // 2. 获取事务绑定的数据库连接
    auto db = trans->getMySQL();
    if (!db) {
        CIM_LOG_ERROR(g_logger) << "EditContactRemark get transaction connection failed, user_id="
                                << user_id << ", contact_id=" << contact_id;
        result.code = 500;
        result.err = "修改联系人备注失败";
        return result;
    }

    // 3. 执行修改联系人备注操作，使用事务绑定的数据库连接
    if (!CIM::dao::ContactDAO::EditRemark(db, user_id, contact_id, remark, &err)) {
        if (!err.empty()) {
            trans->rollback();  // 回滚事务
            CIM_LOG_ERROR(g_logger)
                << "EditContactRemark failed, user_id=" << user_id << ", err=" << err;
            result.code = 500;
            result.err = "修改联系人备注失败";
            return result;
        }
    }

    // 4. 修改会话表备注
    if (!CIM::dao::TalkSessionDAO::EditRemarkWithConn(db, user_id, contact_id, remark, &err)) {
        if (!err.empty()) {
            trans->rollback();  // 回滚事务
            CIM_LOG_ERROR(g_logger)
                << "EditContactRemark EditConversationRemark failed, user_id=" << user_id
                << ", contact_id=" << contact_id << ", err=" << err;
            result.code = 500;
            result.err = "修改联系人备注失败";
            return result;
        }
    }

    // 5. 提交事务
    if (!trans->commit()) {
        const auto commit_err = db->getErrStr();
        trans->rollback();  // 回滚事务
        CIM_LOG_ERROR(g_logger) << "EditContactRemark commit transaction failed, user_id="
                                << user_id << ", contact_id=" << contact_id;
        result.code = 500;
        result.err = "修改联系人备注失败";
        return result;
    }

    result.ok = true;
    return result;
}

VoidResult ContactService::DeleteContact(const uint64_t user_id, const uint64_t contact_id) {
    VoidResult result;
    std::string err;

    // 1. 创建事务
    auto trans = CIM::MySQLMgr::GetInstance()->openTransaction(kDBName, false);
    if (!trans) {
        CIM_LOG_ERROR(g_logger) << "DeleteContact openTransaction failed, user_id=" << user_id
                                << ", contact_id=" << contact_id;
        result.code = 500;
        result.err = "删除联系人失败";
        return result;
    }

    // 2. 获取事务绑定的数据库连接
    auto db = trans->getMySQL();
    if (!db) {
        CIM_LOG_ERROR(g_logger) << "DeleteContact get transaction connection failed, user_id="
                                << user_id << ", contact_id=" << contact_id;
        result.code = 500;
        result.err = "删除联系人失败";
        return result;
    }

    // 3. 查询联系人所在分组
    uint64_t group_id = 0;
    if (!CIM::dao::ContactDAO::GetOldGroupIdWithConn(db, user_id, contact_id, group_id, &err)) {
        trans->rollback();  // 回滚事务
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger) << "DeleteContact GetGroup failed, user_id=" << user_id
                                    << ", contact_id=" << contact_id << ", err=" << err;
            result.code = 500;
            result.err = "获取联系人分组失败";
            return result;
        }
    }

    // 4. 如果在分组中，更新分组下的联系人数量 -1
    if (group_id != 0) {
        if (!CIM::dao::ContactGroupDAO::UpdateContactCountWithConn(db, group_id, false, &err)) {
            trans->rollback();  // 回滚事务
            if (!err.empty()) {
                CIM_LOG_ERROR(g_logger)
                    << "DeleteContact UpdateContactCount failed, user_id=" << user_id
                    << ", contact_id=" << contact_id << ", group_id=" << group_id
                    << ", err=" << err;
                result.code = 500;
                result.err = "更新联系人分组数量失败";
                return result;
            }
        }
    }

    // 4. 删除 user_id -> contact_id
    if (!CIM::dao::ContactDAO::DeleteWithConn(db, user_id, contact_id, &err)) {
        trans->rollback();  // 回滚事务
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger) << "DeleteContact failed, user_id=" << user_id
                                    << ", contact_id=" << contact_id << ", err=" << err;
            result.code = 500;
            result.err = "删除联系人失败";
            return result;
        }
    }

    // 5. 删除 contact_id -> user_id（双向）
    if (!CIM::dao::ContactDAO::DeleteWithConn(db, contact_id, user_id, &err)) {
        trans->rollback();  // 回滚事务
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger) << "DeleteContact failed, user_id=" << user_id
                                    << ", contact_id=" << contact_id << ", err=" << err;
            result.code = 500;
            result.err = "删除联系人失败";
            return result;
        }
    }

    // 6. 从分组中移除联系人
    if (!CIM::dao::ContactDAO::RemoveFromGroupWithConn(db, user_id, contact_id, &err)) {
        trans->rollback();  // 回滚事务
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger) << "DeleteContact failed, user_id=" << user_id
                                    << ", contact_id=" << contact_id << ", err=" << err;
            result.code = 500;
            result.err = "从分组中移除联系人失败";
            return result;
        }
    }

    // 7. 提交事务
    if (!trans->commit()) {
        CIM_LOG_ERROR(g_logger) << "DeleteContact commit failed, user_id=" << user_id
                                << ", contact_id=" << contact_id;
        result.code = 500;
        result.err = "删除联系人失败";
        return result;
    }

    result.ok = true;
    return result;
}

VoidResult ContactService::SaveContactGroup(
    const uint64_t user_id,
    const std::vector<std::tuple<uint64_t, uint64_t, std::string>>& groupItems) {
    VoidResult result;
    std::string err;
    std::unordered_set<uint64_t> ids_seen;

    // 1. 创建整体事务
    auto trans = CIM::MySQLMgr::GetInstance()->openTransaction(kDBName, false);
    if (!trans) {
        CIM_LOG_ERROR(g_logger) << "SaveContactGroup openTransaction failed, user_id=" << user_id;
        result.code = 500;
        result.err = "保存联系人分组失败";
        return result;
    }
    auto db = trans->getMySQL();
    if (!db) {
        CIM_LOG_ERROR(g_logger) << "SaveContactGroup get transaction connection failed, user_id="
                                << user_id;
        result.code = 500;
        result.err = "保存联系人分组失败";
        return result;
    }

    // 2. 新增/更新分组
    for (const auto& item : groupItems) {
        if (std::get<0>(item) == 0) {
            // id 为0，表示新建分组
            CIM::dao::ContactGroup new_group;
            new_group.user_id = user_id;
            new_group.name = std::get<2>(item);
            new_group.sort = std::get<1>(item);
            if (!CIM::dao::ContactGroupDAO::CreateWithConn(db, new_group, new_group.id, &err)) {
                trans->rollback();
                if (!err.empty()) {
                    CIM_LOG_ERROR(g_logger) << "SaveContactGroup failed, user_id=" << user_id
                                            << ", id=" << std::get<0>(item) << ", err=" << err;
                    result.code = 500;
                    result.err = "保存联系人分组失败";
                    return result;
                }
            }
            ids_seen.insert(new_group.id);  // 记录已见ID
        } else {
            ids_seen.insert(std::get<0>(item));  // 记录已见ID
            // id 不为0，表示更新分组
            if (!CIM::dao::ContactGroupDAO::UpdateWithConn(db, std::get<0>(item), std::get<1>(item),
                                                           std::get<2>(item), &err)) {
                trans->rollback();
                if (!err.empty()) {
                    CIM_LOG_ERROR(g_logger) << "SaveContactGroup failed, user_id=" << user_id
                                            << ", id=" << std::get<0>(item) << ", err=" << err;
                    result.code = 500;
                    result.err = "保存联系人分组失败";
                    return result;
                }
            }
        }
    }

    // 3. 查询用户现有的分组列表（使用事务连接）
    std::vector<CIM::dao::ContactGroupItem> existing_groups;
    if (!CIM::dao::ContactGroupDAO::ListByUserIdWithConn(db, user_id, existing_groups, &err)) {
        trans->rollback();
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger)
                << "SaveContactGroup ListByUserIdWithConn failed, user_id=" << user_id
                << ", err=" << err;
            result.code = 500;
            result.err = "保存联系人分组失败";
            return result;
        }
    }

    // 4. 删除不在传入列表中的分组
    for (const auto& group : existing_groups) {
        if (ids_seen.find(group.id) == ids_seen.end()) {
            // 将分组中的联系人移除
            if (!CIM::dao::ContactDAO::RemoveFromGroupByGroupIdWithConn(db, user_id, group.id,
                                                                        &err)) {
                trans->rollback();
                if (!err.empty()) {
                    CIM_LOG_ERROR(g_logger)
                        << "SaveContactGroup RemoveFromGroupByGroupId failed, user_id=" << user_id
                        << ", id=" << group.id << ", err=" << err;
                    result.code = 500;
                    result.err = "保存联系人分组失败";
                    return result;
                }
            }
            // 删除分组
            if (!CIM::dao::ContactGroupDAO::DeleteWithConn(db, group.id, &err)) {
                trans->rollback();
                if (!err.empty()) {
                    CIM_LOG_ERROR(g_logger) << "SaveContactGroup Delete failed, user_id=" << user_id
                                            << ", id=" << group.id << ", err=" << err;
                    result.code = 500;
                    result.err = "保存联系人分组失败";
                    return result;
                }
            }
        }
    }

    // 5. 提交事务
    if (!trans->commit()) {
        CIM_LOG_ERROR(g_logger) << "SaveContactGroup commit failed, user_id=" << user_id;
        result.code = 500;
        result.err = "保存联系人分组失败";
        return result;
    }

    result.ok = true;
    return result;
}

VoidResult ContactService::ChangeContactGroup(const uint64_t user_id, const uint64_t contact_id,
                                              const uint64_t group_id) {
    VoidResult result;
    std::string err;

    // 1. 创建事务
    auto trans = CIM::MySQLMgr::GetInstance()->openTransaction(kDBName, false);
    if (!trans) {
        CIM_LOG_ERROR(g_logger) << "DeleteContact openTransaction failed, user_id=" << user_id
                                << ", contact_id=" << contact_id;
        result.code = 500;
        result.err = "修改联系人分组失败";
        return result;
    }

    // 2. 获取事务绑定的数据库连接
    auto db = trans->getMySQL();
    if (!db) {
        CIM_LOG_ERROR(g_logger) << "DeleteContact get transaction connection failed, user_id="
                                << user_id << ", contact_id=" << contact_id;
        result.code = 500;
        result.err = "修改联系人分组失败";
        return result;
    }

    // 3. 查询好友原先的分组
    uint64_t old_group_id = 0;
    if (!CIM::dao::ContactDAO::GetOldGroupIdWithConn(db, user_id, contact_id, old_group_id, &err)) {
        trans->rollback();  // 回滚事务
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger)
                << "ChangeContactGroup GetGroup failed, contact_id=" << contact_id
                << ", err=" << err;
            result.code = 500;
            result.err = "获取联系人分组失败";
            return result;
        }
    }

    // 4. 修改联系人分组
    if (!CIM::dao::ContactDAO::ChangeGroupWithConn(db, user_id, contact_id, group_id, &err)) {
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger) << "ChangeContactGroup failed, contact_id=" << contact_id
                                    << ", group_id=" << group_id << ", err=" << err;
            result.code = 500;
            result.err = "修改联系人分组失败";
            return result;
        }
    }

    if (old_group_id != 0) {
        // 原先分组下的联系人数量 -1
        if (!CIM::dao::ContactGroupDAO::UpdateContactCountWithConn(db, old_group_id, false, &err)) {
            trans->rollback();  // 回滚事务
            if (!err.empty()) {
                CIM_LOG_ERROR(g_logger)
                    << "ChangeContactGroup UpdateContactCount failed, contact_id=" << contact_id
                    << ", group_id=" << old_group_id << ", err=" << err;
                result.code = 500;
                result.err = "修改联系人分组失败";
                return result;
            }
        }
    }

    // 当前分组下的联系人数量 +1
    if (!CIM::dao::ContactGroupDAO::UpdateContactCountWithConn(db, group_id, true, &err)) {
        trans->rollback();  // 回滚事务
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger)
                << "ChangeContactGroup UpdateContactCount failed, contact_id=" << contact_id
                << ", group_id=" << group_id << ", err=" << err;
            result.code = 500;
            result.err = "修改联系人分组失败";
            return result;
        }
    }

    // 5. 提交事务
    if (!trans->commit()) {
        CIM_LOG_ERROR(g_logger) << "ChangeContactGroup commit failed, contact_id=" << contact_id;
        result.code = 500;
        result.err = "修改联系人分组失败";
        return result;
    }

    result.ok = true;
    return result;
}

}  // namespace CIM::app