#ifndef __CIM_APP_CONTACT_SERVICE_HPP__
#define __CIM_APP_CONTACT_SERVICE_HPP__

#include "dao/contact_apply_dao.hpp"
#include "dao/contact_dao.hpp"
#include "dao/contact_group_dao.hpp"
#include "dao/user_dao.hpp"
#include "result.hpp"

namespace CIM::app {
using UserResult = Result<CIM::dao::User>;
using ContactDetailsResult = Result<CIM::dao::ContactDetails>;
using ContactListResult = Result<std::vector<CIM::dao::ContactItem>>;
using ContactAddResult = Result<CIM::dao::ContactApply>;
using ApplyCountResult = Result<uint64_t>;
using ContactApplyListResult = Result<std::vector<CIM::dao::ContactApplyItem>>;
using ResultVoid = Result<std::string>;

class ContactService {
   public:
    // 根据手机号查询联系人
    static UserResult SearchByMobile(const std::string& mobile);

    // 根据用户ID获取联系人详情
    static ContactDetailsResult GetContactDetail(const uint64_t owner_id, const uint64_t target_id);

    // 显示好友列表
    static ContactListResult ListFriends(const uint64_t user_id);

    // 创建添加联系人申请
    static ContactAddResult CreateContactApply(uint64_t from_id, uint64_t to_id,
                                               const std::string& remark);

    // 查询添加联系人申请未处理数量
    static ApplyCountResult GetPendingContactApplyCount(uint64_t user_id);

    // 获取好友申请列表
    static ContactApplyListResult ListContactApplies(uint64_t user_id);

    // 同意好友申请
    static ResultVoid AgreeApply(uint64_t apply_id, std::string remark);

    // 拒绝好友申请
    static ResultVoid RejectApply(uint64_t apply_id, std::string remark);
};

}  // namespace CIM::app

#endif  // __CIM_APP_CONTACT_SERVICE_HPP__