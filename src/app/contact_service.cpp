#include "app/contact_service.hpp"

#include <vector>

#include "dao/contact_apply_dao.hpp"
#include "dao/contact_dao.hpp"
#include "dao/user_dao.hpp"
#include "macro.hpp"

namespace CIM::app {
static auto g_logger = CIM_LOG_NAME("system");

UserResult ContactService::SearchByMobile(const std::string& mobile) {
    UserResult result;

    CIM::dao::User user;
    if (!CIM::dao::UserDAO::GetByMobile(mobile, user)) {
        result.code = 404;
        result.err = "联系人不存在！";
        return result;
    }

    result.ok = true;
    result.data = std::move(user);
    return result;
}

ContactDetailsResult ContactService::GetContactDetail(const uint64_t owner_id,
                                                      const uint64_t target_id) {
    ContactDetailsResult result;

    CIM::dao::ContactDetails contact;
    if (!CIM::dao::ContactDAO::GetByOwnerAndTarget(owner_id, target_id, contact)) {
        result.code = 404;
        result.err = "用户不存在！";
        return result;
    }

    result.ok = true;
    result.data = std::move(contact);
    return result;
}

ContactListResult ContactService::ListFriends(const uint64_t user_id) {
    ContactListResult result;
    std::string err;

    std::vector<CIM::dao::ContactItem> items;
    if (!CIM::dao::ContactDAO::ListByUser(user_id, items, &err)) {
        CIM_LOG_ERROR(g_logger) << "ListFriends failed, user_id=" << user_id << ", err=" << err;
        result.code = 500;
        result.err = "获取好友列表失败！";
        return result;
    }
    result.ok = true;
    result.data = std::move(items);
    return result;
}

ContactAddResult ContactService::CreateContactApply(uint64_t from_id, uint64_t to_id,
                                                    const std::string& remark) {
    ContactAddResult result;
    std::string err;

    CIM::dao::ContactApply apply;
    apply.applicant_id = from_id;
    apply.target_id = to_id;
    apply.remark = remark;
    apply.created_at = TimeUtil::NowToS();
    if (!CIM::dao::ContactApplyDAO::Create(apply, apply.id, &err)) {
        CIM_LOG_ERROR(g_logger) << "CreateContactApply failed, from_id=" << from_id
                                << ", to_id=" << to_id << ", err=" << err;
        result.code = 500;
        result.err = "创建好友申请失败！";
        return result;
    }

    result.ok = true;
    result.data = std::move(apply);
    return result;
}

ApplyCountResult ContactService::GetPendingContactApplyCount(uint64_t user_id) {
    ApplyCountResult result;
    std::string err;

    uint64_t count = 0;
    if (!CIM::dao::ContactApplyDAO::GetPendingCountById(user_id, count, &err)) {
        CIM_LOG_ERROR(g_logger) << "GetPendingContactApplyCount failed, user_id=" << user_id
                                << ", err=" << err;
        result.code = 500;
        result.err = "获取未处理的好友申请数量失败！";
        return result;
    }

    result.ok = true;
    result.data = count;
    return result;
}

ContactApplyListResult ContactService::ListContactApplies(uint64_t user_id) {
    ContactApplyListResult result;

    std::vector<CIM::dao::ContactApplyItem> items;
    std::string err;
    if (!CIM::dao::ContactApplyDAO::GetItemById(user_id, items, &err)) {
        CIM_LOG_ERROR(g_logger) << "ListContactApplies failed, user_id=" << user_id
                                << ", err=" << err;
        result.code = 500;
        result.err = "获取好友申请列表失败！";
        return result;
    }
    result.ok = true;
    result.data = std::move(items);
    return result;
}

ResultVoid ContactService::AgreeApply(uint64_t apply_id, std::string remark) {
    ResultVoid result;
    std::string err;

    if (!CIM::dao::ContactApplyDAO::AgreeApply(apply_id, remark, &err)) {
        CIM_LOG_ERROR(g_logger) << "HandleContactApply AgreeApply failed, apply_id=" << apply_id
                                << ", err=" << err;
        result.code = 500;
        result.err = "处理好友申请失败！";
        return result;
    }

    CIM::dao::ContactApply apply;
    if (!CIM::dao::ContactApplyDAO::GetDetailById(apply_id, apply, &err)) {
        CIM_LOG_ERROR(g_logger) << "HandleContactApply GetDetailById failed, apply_id=" << apply_id
                                << ", err=" << err;
        result.code = 500;
        result.err = "获取好友申请详情失败！";
        return result;
    }

    // 插入双向联系人记录
    std::vector<CIM::dao::Contact> contacts_to_create;

    // 目标用户添加申请人
    CIM::dao::Contact contact1;
    contact1.user_id = apply.target_id;
    contact1.contact_id = apply.applicant_id;
    contact1.relation = 2;
    contact1.group_id = 0;
    contact1.created_at = TimeUtil::NowToS();
    contact1.status = 1;
    contacts_to_create.push_back(contact1);

    // 申请人添加目标用户
    CIM::dao::Contact contact2;
    contact2.user_id = apply.applicant_id;
    contact2.contact_id = apply.target_id;
    contact2.relation = 2;
    contact2.group_id = 0;
    contact2.created_at = TimeUtil::NowToS();
    contact2.status = 1;
    contacts_to_create.push_back(contact2);

    for (const auto& contact : contacts_to_create) {
        if (!CIM::dao::ContactDAO::Create(contact, &err)) {
            CIM_LOG_ERROR(g_logger)
                << "CreateContactRecordsForApply failed, apply_id=" << apply_id << ", err=" << err;
            result.code = 500;
            result.err = "创建好友记录失败！";
            return result;
        }
    }

    result.ok = true;
    return result;
}

ResultVoid ContactService::RejectApply(uint64_t apply_id, std::string remark) {
    ResultVoid result;
    std::string err;

    if (!CIM::dao::ContactApplyDAO::RejectApply(apply_id, remark, &err)) {
        CIM_LOG_ERROR(g_logger) << "HandleContactApply RejectApply failed, apply_id=" << apply_id
                                << ", err=" << err;
        result.code = 500;
        result.err = "处理好友申请失败！";
        return result;
    }

    result.ok = true;
    return result;
}
}  // namespace CIM::app