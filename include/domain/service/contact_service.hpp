#ifndef __IM_DOMAIN_SERVICE_CONTACT_SERVICE_HPP__
#define __IM_DOMAIN_SERVICE_CONTACT_SERVICE_HPP__

#include <memory>
#include <vector>

#include "common/result.hpp"
#include "dto/contact_dto.hpp"
#include "dto/talk_dto.hpp"
#include "model/user.hpp"

namespace IM::domain::service {

class IContactService {
   public:
    using Ptr = std::shared_ptr<IContactService>;
    virtual ~IContactService() = default;

    // 同意好友申请
    virtual Result<dto::TalkSessionItem> AgreeApply(const uint64_t user_id, const uint64_t apply_id,
                                                    const std::string& remark) = 0;

    // 根据手机号查询联系人
    virtual Result<model::User> SearchByMobile(const std::string& mobile) = 0;

    // 根据用户ID获取联系人详情
    virtual Result<dto::ContactDetails> GetContactDetail(const uint64_t user_id,
                                                         const uint64_t target_id) = 0;

    // 显示好友列表
    virtual Result<std::vector<dto::ContactItem>> ListFriends(const uint64_t user_id) = 0;

    // 创建添加联系人申请
    virtual Result<void> CreateContactApply(const uint64_t apply_user_id,
                                            const uint64_t target_user_id,
                                            const std::string& remark) = 0;

    // 查询添加联系人申请未处理数量
    virtual Result<uint64_t> GetPendingContactApplyCount(const uint64_t user_id) = 0;

    // 获取好友申请列表
    virtual Result<std::vector<dto::ContactApplyItem>> ListContactApplies(
        const uint64_t user_id) = 0;

    // 拒绝好友申请
    virtual Result<void> RejectApply(const uint64_t handler_user_id, const uint64_t apply_user_id,
                                     const std::string& remark) = 0;

    // 修改联系人备注
    virtual Result<void> EditContactRemark(const uint64_t user_id, const uint64_t contact_id,
                                           const std::string& remark) = 0;

    // 删除联系人（软删除）
    virtual Result<void> DeleteContact(const uint64_t user_id, const uint64_t contact_id) = 0;

    // 保存好友分组
    virtual Result<void> SaveContactGroup(
        const uint64_t user_id,
        const std::vector<std::tuple<uint64_t, uint64_t, std::string>>& groupItems) = 0;

    // 获取好友分组列表
    virtual Result<std::vector<dto::ContactGroupItem>> GetContactGroupLists(
        const uint64_t user_id) = 0;

    // 修改联系人分组
    virtual Result<void> ChangeContactGroup(const uint64_t user_id, const uint64_t contact_id,
                                            const uint64_t group_id) = 0;
};

}  // namespace IM::domain::service

#endif  // __IM_DOMAIN_SERVICE_CONTACT_SERVICE_HPP__