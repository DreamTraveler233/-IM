#pragma once
#include <string>

#include "base/macro.hpp"

namespace IM::model {

struct Group {
    uint64_t id = 0;
    std::string group_name;       // 群组名称
    std::string avatar;           // 群头像URL
    std::string avatar_media_id;  // 群头像媒体ID
    std::string profile;          // 群简介
    uint64_t leader_id = 0;       // 群主ID
    uint64_t creator_id = 0;      // 创建者ID
    int is_mute = 2;              // 是否全员禁言 1:是 2:否
    int is_overt = 1;             // 是否公开 1:否 2:是
    int overt_type = 0;           // 公开类型
    int max_num = 500;            // 最大成员数
    int member_num = 1;           // 当前成员数
    int is_dismissed = 0;         // 是否已解散
    std::string dismissed_at;     // 解散时间
    std::string created_at;       // 创建时间
    std::string updated_at;       // 更新时间
};

}  // namespace IM::model
