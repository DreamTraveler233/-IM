#ifndef __CIM_APP_CONTACT_SERVICE_HPP__
#define __CIM_APP_CONTACT_SERVICE_HPP__

#include "dao/contact_apply_dao.hpp"
#include "dao/contact_dao.hpp"
#include "dao/contact_group_dao.hpp"
#include "dao/user_dao.hpp"
#include "result.hpp"

namespace CIM::app {

class ContactService {
   public:
    // 同意好友申请（同时在服务端创建会话并返回会话信息）
    static TalkSessionResult AgreeApply(const uint64_t user_id, const uint64_t apply_id,
                                       const std::string& remark);
    // 根据手机号查询联系人
    static UserResult SearchByMobile(const std::string& mobile);

    // 根据用户ID获取联系人详情
    static ContactDetailsResult GetContactDetail(const uint64_t target_id);

    // 显示好友列表
    static ContactListResult ListFriends(const uint64_t user_id);

    // 创建添加联系人申请
    static VoidResult CreateContactApply(const uint64_t apply_user_id,
                                         const uint64_t target_user_id, const std::string& remark);

    // 查询添加联系人申请未处理数量
    static ApplyCountResult GetPendingContactApplyCount(const uint64_t user_id);

    // 获取好友申请列表
    static ContactApplyListResult ListContactApplies(const uint64_t user_id);

    // 拒绝好友申请
    static VoidResult RejectApply(const uint64_t handler_user_id, const uint64_t apply_user_id,
                                  const std::string& remark);

    // 修改联系人备注
    static VoidResult EditContactRemark(const uint64_t user_id, const uint64_t contact_id,
                                        const std::string& remark);

    // 删除联系人（软删除）
    static VoidResult DeleteContact(const uint64_t user_id, const uint64_t contact_id);

    // 保存好友分组
    static VoidResult SaveContactGroup(
        const uint64_t user_id,
        const std::vector<std::tuple<uint64_t, uint64_t, std::string>>& groupItems);

    // 获取好友分组列表
    static ContactGroupListResult GetContactGroupLists(const uint64_t user_id);

    // 修改联系人分组
    static VoidResult ChangeContactGroup(const uint64_t user_id, const uint64_t contact_id,
                                         const uint64_t group_id);
};

}  // namespace CIM::app

#endif  // __CIM_APP_CONTACT_SERVICE_HPP__