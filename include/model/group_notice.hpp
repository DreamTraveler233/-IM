#pragma once
#include <string>

#include "base/macro.hpp"

namespace IM::model {

struct GroupNotice {
    uint64_t group_id = 0;
    std::string content;         // 公告内容
    uint64_t modify_user_id = 0; // 修改人ID
    std::string created_at;      // 创建时间
    std::string updated_at;      // 更新时间
};

}  // namespace IM::model
