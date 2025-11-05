#pragma once

#include <cstdint>
#include <string>
#include <ctime>
#include <vector>

namespace CIM::dao {

struct ContactGroup {
    uint64_t id = 0;              // 分组ID
    uint64_t user_id = 0;         // 所属用户ID
    std::string name;             // 分组名称
    uint32_t sort = 100;          // 排序值
    uint32_t contact_count = 0;   // 分组下联系人数量
    std::time_t created_at = 0;   // 创建时间
    std::time_t updated_at = 0;   // 更新时间
};

class ContactGroupDAO {
   public:
    static bool Create(const ContactGroup& g, uint64_t& out_id, std::string* err = nullptr);
    static bool GetById(uint64_t id, ContactGroup& out);
    static bool ListByUser(uint64_t user_id, std::vector<ContactGroup>& outs);
    static bool Update(uint64_t id, const std::string& name, uint32_t sort, std::string* err = nullptr);
    static bool Delete(uint64_t id, std::string* err = nullptr);
};

}  // namespace CIM::dao
