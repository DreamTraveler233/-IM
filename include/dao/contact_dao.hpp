#pragma once

#include <cstdint>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include "db/mysql.hpp"

namespace CIM::dao {

struct Contact {
    uint64_t id = 0;              // 联系人ID
    uint64_t owner_user_id = 0;   // 联系人拥有者（发起展示视角的用户）
    uint64_t friend_user_id = 0;  // 好友用户ID
    uint64_t group_id = 0;        // 联系人分组ID（im_contact_group.id）
    std::string remark = "";      // 我对该联系人的备注名
    uint8_t status = 1;           // 状态，1正常，2删除
    uint8_t relation = 1;         // 我视角的关系，1=陌生人，2=好友，3=企业同事，4=本人
    uint8_t is_block = 0;         // 是否拉黑，0否，1是
    std::time_t created_at = 0;   // 创建时间（成为好友时间）
    std::time_t updated_at = 0;   // 更新时间
    std::time_t deleted_at = 0;   // 软删除时间
};

struct ContactDetails {
    uint64_t user_id = 0;           // 用户ID
    std::string avatar;             // 头像
    uint8_t gender = 0;             // 性别
    std::string mobile;             // 手机号
    std::string motto;              // 个性签名
    std::string nickname;           // 昵称
    std::string email;              // 邮箱
    uint8_t relation = 1;           // 关系
    uint32_t contact_group_id = 0;  // 分组ID
    std::string contact_remark;     // 备注
};

struct ContactItem {
    uint64_t user_id = 0;   // 用户ID
    std::string nickname;   // 昵称
    uint8_t gender = 0;     // 性别
    std::string motto;      // 个性签名
    std::string avatar;     // 头像
    std::string remark;     // 备注
    uint64_t group_id = 0;  // 分组ID
};

class ContactDAO {
   public:
    // 获取好友列表
    static bool ListByUser(const uint64_t user_id, std::vector<ContactItem>& out,
                           std::string* err = nullptr);

    // 根据用户ID和目标ID获取联系人详情
    static bool GetByOwnerAndTarget(const uint64_t target_id, ContactDetails& out,
                                    std::string* err = nullptr);

    // 根据用户ID和目标ID获取联系人详情(使用已有的 MySQL 连接)
    static bool GetByOwnerAndTargetWithConn(const std::shared_ptr<CIM::MySQL>& db,
                                            const uint64_t owner_id, const uint64_t target_id,
                                            ContactDetails& out, std::string* err = nullptr);

    // 使用已有的 MySQL 连接执行 Upsert，便于包裹事务
    static bool UpsertWithConn(const std::shared_ptr<CIM::MySQL>& db, const Contact& c,
                               std::string* err = nullptr);

    // 修改联系人备注
    static bool EditRemark(const std::shared_ptr<CIM::MySQL>& db, const uint64_t user_id,
                           const uint64_t contact_id, const std::string& remark,
                           std::string* err = nullptr);

    // 删除联系人
    static bool DeleteWithConn(const std::shared_ptr<CIM::MySQL>& db, const uint64_t user_id,
                               const uint64_t contact_id, std::string* err = nullptr);

    // 修改好友状态与好友关系
    static bool UpdateStatusAndRelationWithConn(const std::shared_ptr<CIM::MySQL>& db,
                                                const uint64_t user_id, const uint64_t contact_id,
                                                const uint8_t status, const uint8_t relation,
                                                std::string* err = nullptr);

    // 修改联系人分组
    static bool ChangeGroupWithConn(const std::shared_ptr<CIM::MySQL>& db, const uint64_t user_id,
                                    const uint64_t contact_id, const uint64_t group_id,
                                    std::string* err = nullptr);

    // 获取好友原先的分组
    static bool GetOldGroupIdWithConn(const std::shared_ptr<CIM::MySQL>& db, const uint64_t user_id,
                                      const uint64_t contact_id, uint64_t& out_group_id,
                                      std::string* err = nullptr);

    // 当删除好友时，将该好友从所属分组移出（group_id 设为 0）
    static bool RemoveFromGroupWithConn(const std::shared_ptr<CIM::MySQL>& db,
                                        const uint64_t user_id, const uint64_t contact_id,
                                        std::string* err = nullptr);

    // 当分组删除时，将在该分组的所有好友移出（group_id 设为 0）
    static bool RemoveFromGroupByGroupIdWithConn(const std::shared_ptr<CIM::MySQL>& db,
                                                 const uint64_t user_id, const uint64_t group_id,
                                                 std::string* err = nullptr);
};

}  // namespace CIM::dao
