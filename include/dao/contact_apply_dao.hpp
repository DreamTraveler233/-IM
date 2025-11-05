#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace CIM::dao {

struct ContactApply {
    uint64_t id = 0;             // 申请ID
    uint64_t applicant_id = 0;   // 申请人ID
    uint64_t target_id = 0;      // 被申请人ID
    std::string remark;          // 申请备注
    uint8_t status = 0;          // 0待处理 1同意 2拒绝
    std::string handle_remark;   // 处理备注
    std::time_t handled_at = 0;  // 处理时间
    std::time_t created_at = 0;  // 创建时间
    std::time_t updated_at = 0;  // 更新时间
};

struct ContactApplyItem {
    uint64_t id = 0;         // 申请记录ID
    uint64_t user_id = 0;    // 用户ID
    uint64_t friend_id = 0;  // 好友ID
    std::string remark;      // 申请备注
    std::string nickname;    // 好友昵称
    std::string avatar;      // 好友头像
    std::string created_at;  // 申请时间
};

class ContactApplyDAO {
   public:
    // 创建添加联系人申请
    static bool Create(const ContactApply& a, uint64_t& out_id, std::string* err = nullptr);
    // 根据ID获取申请未处理数
    static bool GetPendingCountById(uint64_t id, uint64_t& out_count, std::string* err = nullptr);
    // 根据ID获取未处理的好友申请记录
    static bool GetItemById(uint64_t id, std::vector<ContactApplyItem>& out, std::string* err = nullptr);
    // 同意好友申请
    static bool AgreeApply(uint64_t apply_id, const std::string& remark, std::string* err = nullptr);
    // 拒接好友申请
    static bool RejectApply(uint64_t apply_id, const std::string& remark, std::string* err = nullptr);
    // 根据ID获取申请记录详情
    static bool GetDetailById(uint64_t apply_id, ContactApply& out, std::string* err = nullptr);
};

}  // namespace CIM::dao
