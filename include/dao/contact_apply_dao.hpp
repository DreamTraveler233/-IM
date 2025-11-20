#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>
#include <memory>

#include "db/mysql.hpp"

namespace CIM::dao {

struct ContactApply {
    uint64_t id = 0;                 // 主键ID
    uint64_t apply_user_id = 0;      // 申请人用户ID
    uint64_t target_user_id = 0;     // 被申请用户ID
    std::string remark = "";         // 申请附言（打招呼）
    uint8_t status = 1;              // 状态：1=待处理 2=已同意 3=已拒绝
    uint64_t handler_user_id = 0;    // 处理人用户ID（通常等于 target_user_id）
    std::string handle_remark = "";  // 处理备注（可选）
    std::time_t handled_at = 0;      // 处理时间（同意/拒绝）
    std::time_t created_at = 0;      // 申请时间
    std::time_t updated_at = 0;      // 更新时间
};

struct ContactApplyItem {
    uint64_t id = 0;              // 申请记录ID
    uint64_t apply_user_id = 0;   // 申请人用户ID
    uint64_t target_user_id = 0;  // 被申请用户ID
    std::string remark = "";      // 申请备注
    std::string nickname = "";    // 好友昵称
    std::string avatar = "";      // 好友头像
    std::string created_at = "";  // 申请时间
};

class ContactApplyDAO {
   public:
    // 创建添加联系人申请
    static bool Create(const ContactApply& a, std::string* err = nullptr);
    // 根据ID获取申请未处理数
    static bool GetPendingCountById(const uint64_t id, uint64_t& out_count,
                                    std::string* err = nullptr);
    // 根据ID获取未处理的好友申请记录
    static bool GetItemById(const uint64_t id, std::vector<ContactApplyItem>& out,
                            std::string* err = nullptr);
    // 同意好友申请
    static bool AgreeApplyWithConn(const std::shared_ptr<CIM::MySQL>& db, const uint64_t user_id,
                                   const uint64_t apply_id, const std::string& remark,
                                   std::string* err = nullptr);
    // 拒接好友申请
    static bool RejectApply(const uint64_t handler_user_id, const uint64_t apply_user_id,
                            const std::string& remark, std::string* err = nullptr);
    // 根据ID获取申请记录详情（使用带MySQL连接的函数）
    static bool GetDetailByIdWithConn(const std::shared_ptr<CIM::MySQL>& db,
                                      const uint64_t apply_id, ContactApply& out,
                                      std::string* err = nullptr);
    // 根据ID获取申请记录详情（内部会自动获取 MySQL 连接）
    static bool GetDetailById(const uint64_t apply_id, ContactApply& out,
                              std::string* err = nullptr);
};

}  // namespace CIM::dao
