#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace CIM::dao {

struct Contact {
    uint64_t id = 0;             // 联系人ID
    uint64_t user_id = 0;        // 用户ID
    uint64_t contact_id = 0;     // 联系人用户ID
    uint8_t relation = 1;        // 关系
    uint64_t group_id = 0;       // 分组ID
    std::string remark;          // 备注
    std::time_t created_at = 0;  // 创建时间
    std::time_t updated_at = 0;  // 更新时间
    uint8_t status = 1;          // 状态，1正常，0删除
};

struct ContactDetails {
    uint64_t user_id = 0;           // 用户ID
    std::string avatar;             // 头像
    uint32_t gender = 0;            // 性别
    std::string mobile;             // 手机号
    std::string motto;              // 个性签名
    std::string nickname;           // 昵称
    std::string email;              // 邮箱
    uint32_t relation = 1;          // 关系
    uint32_t contact_group_id = 0;  // 分组ID
    std::string contact_remark;     // 备注
};

struct ContactItem {
    uint64_t user_id = 0;   // 用户ID
    std::string nickname;   // 昵称
    uint32_t gender = 0;    // 性别
    std::string motto;      // 个性签名
    std::string avatar;     // 头像
    std::string remark;     // 备注
    uint64_t group_id = 0;  // 分组ID
};

class ContactDAO {
   public:
    // 获取好友列表
    static bool ListByUser(uint64_t user_id, std::vector<ContactItem>& out,
                           std::string* err = nullptr);
    // 根据用户ID获取联系人详情
    static bool GetByOwnerAndTarget(uint64_t owner_id, uint64_t target_id, ContactDetails& out,
                                    std::string* err = nullptr);
    // 创建联系人记录
    static bool Create(const Contact& c, std::string* err = nullptr);
};

}  // namespace CIM::dao
