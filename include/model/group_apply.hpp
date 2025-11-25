#pragma once
#include <string>

#include "base/macro.hpp"

namespace IM::model {

struct GroupApply {
    uint64_t id = 0;
    uint64_t group_id = 0;
    uint64_t user_id = 0;
    std::string remark;          // 申请备注
    int status = 1;              // 状态 1:待处理 2:已同意 3:已拒绝
    int is_read = 0;             // 是否已读
    uint64_t handler_user_id = 0;// 处理人ID
    std::string handled_at;      // 处理时间
    std::string created_at;      // 创建时间
    std::string updated_at;      // 更新时间
};

}  // namespace IM::model
