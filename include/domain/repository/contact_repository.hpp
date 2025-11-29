#ifndef __IM_DOMAIN_REPOSITORY_CONTACT_REPOSITORY_HPP__
#define __IM_DOMAIN_REPOSITORY_CONTACT_REPOSITORY_HPP__

#include <memory>
#include <vector>

#include "db/mysql.hpp"
#include "dto/contact_dto.hpp"
#include "dto/user_dto.hpp"
#include "model/contact.hpp"
#include "model/contact_apply.hpp"
#include "model/contact_group.hpp"
#include "model/talk_session.hpp"
#include "model/user.hpp"

namespace IM::domain::repository {

class IContactRepository {
   public:
    using Ptr = std::shared_ptr<IContactRepository>;
    virtual ~IContactRepository() = default;

    // 获取好友列表
    virtual bool GetContactItemListByUserId(const uint64_t user_id,
                                            std::vector<dto::ContactItem>& out,
                                            std::string* err = nullptr) = 0;

    // 根据用户ID和目标ID获取联系人详情
    virtual bool GetByOwnerAndTarget(const uint64_t owner_id, const uint64_t target_id,
                                     dto::ContactDetails& out, std::string* err = nullptr) = 0;

    // 根据用户ID和目标ID获取联系人详情
    virtual bool GetByOwnerAndTarget(const std::shared_ptr<IM::MySQL>& db, const uint64_t owner_id,
                                     const uint64_t target_id, dto::ContactDetails& out,
                                     std::string* err = nullptr) = 0;

    // 使用已有的 MySQL 连接执行 Upsert，便于包裹事务
    virtual bool UpsertContact(const std::shared_ptr<IM::MySQL>& db, const model::Contact& c,
                               std::string* err = nullptr) = 0;

    // 修改联系人备注
    virtual bool EditRemark(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                            const uint64_t contact_id, const std::string& remark,
                            std::string* err = nullptr) = 0;

    // 删除联系人
    virtual bool DeleteContact(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                               const uint64_t contact_id, std::string* err = nullptr) = 0;

    // 修改好友状态与好友关系
    virtual bool UpdateStatusAndRelation(const std::shared_ptr<IM::MySQL>& db,
                                         const uint64_t user_id, const uint64_t contact_id,
                                         const uint8_t status, const uint8_t relation,
                                         std::string* err = nullptr) = 0;

    // 修改联系人分组
    virtual bool ChangeContactGroup(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                                    const uint64_t contact_id, const uint64_t group_id,
                                    std::string* err = nullptr) = 0;

    // 获取好友原先的分组
    virtual bool GetOldGroupId(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                               const uint64_t contact_id, uint64_t& out_group_id,
                               std::string* err = nullptr) = 0;

    // 当删除好友时，将该好友从所属分组移出（group_id 设为 0）
    virtual bool RemoveFromGroup(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                                 const uint64_t contact_id, std::string* err = nullptr) = 0;

    // 当分组删除时，将在该分组的所有好友移出（group_id 设为 0）
    virtual bool RemoveFromGroupByGroupId(const std::shared_ptr<IM::MySQL>& db,
                                          const uint64_t user_id, const uint64_t group_id,
                                          std::string* err = nullptr) = 0;

    // 创建添加联系人申请
    virtual bool CreateContactApply(const model::ContactApply& a, std::string* err = nullptr) = 0;

    // 根据ID获取申请未处理数
    virtual bool GetPendingCountById(const uint64_t id, uint64_t& out_count,
                                     std::string* err = nullptr) = 0;

    // 根据ID获取未处理的好友申请记录
    virtual bool GetContactApplyItemById(const uint64_t id, std::vector<dto::ContactApplyItem>& out,
                                         std::string* err = nullptr) = 0;

    // 同意好友申请
    virtual bool AgreeApply(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                            const uint64_t apply_id, const std::string& remark,
                            std::string* err = nullptr) = 0;

    // 拒接好友申请
    virtual bool RejectApply(const uint64_t handler_user_id, const uint64_t apply_user_id,
                             const std::string& remark, std::string* err = nullptr) = 0;

    // 根据ID获取申请记录详情
    virtual bool GetDetailById(const std::shared_ptr<IM::MySQL>& db, const uint64_t apply_id,
                               model::ContactApply& out, std::string* err = nullptr) = 0;

    // 根据ID获取申请记录详情
    virtual bool GetDetailById(const uint64_t apply_id, model::ContactApply& out,
                               std::string* err = nullptr) = 0;

    // 创建新的联系人分组
    virtual bool CreateContactGroup(const std::shared_ptr<IM::MySQL>& db,
                                    const model::ContactGroup& g, uint64_t& out_id,
                                    std::string* err = nullptr) = 0;

    // 更新联系人分组信息
    virtual bool UpdateContactGroup(const std::shared_ptr<IM::MySQL>& db, const uint64_t id,
                                    const uint32_t sort, const std::string& name,
                                    std::string* err = nullptr) = 0;

    // 根据用户ID获取联系人分组列表
    virtual bool GetContactGroupItemListByUserId(const uint64_t user_id,
                                                 std::vector<dto::ContactGroupItem>& outs,
                                                 std::string* err = nullptr) = 0;

    // 根据用户ID获取联系人分组列表
    virtual bool GetContactGroupItemListByUserId(const std::shared_ptr<IM::MySQL>& db,
                                                 const uint64_t user_id,
                                                 std::vector<dto::ContactGroupItem>& outs,
                                                 std::string* err = nullptr) = 0;

    // 删除联系人分组
    virtual bool DeleteContactGroup(const std::shared_ptr<IM::MySQL>& db, const uint64_t id,
                                    std::string* err = nullptr) = 0;

    // 更新分组下的联系人数量
    virtual bool UpdateContactCount(const std::shared_ptr<IM::MySQL>& db, const uint64_t group_id,
                                    bool increase, std::string* err = nullptr) = 0;
};

}  // namespace IM::domain::repository

#endif  // __IM_DOMAIN_REPOSITORY_CONTACT_REPOSITORY_HPP__