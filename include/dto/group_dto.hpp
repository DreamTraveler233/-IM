#pragma once
#include <string>
#include <vector>

#include "base/macro.hpp"

namespace IM::dto {

struct GroupApplyItem {
    uint64_t id = 0;
    uint64_t user_id = 0;
    uint64_t group_id = 0;
    std::string remark; // 申请备注
    std::string avatar; // 申请人头像
    std::string nickname; // 申请人昵称
    std::string created_at; // 申请时间
    std::string group_name; // 群组名称
};

struct GroupItem {
    uint64_t group_id = 0;
    std::string group_name;
    std::string avatar;
    std::string profile; // 群简介
    uint64_t leader = 0; // 群主ID
    uint64_t creator_id = 0; // 创建者ID
};

struct GroupMemberItem {
    uint64_t user_id = 0;
    std::string nickname;
    std::string avatar;
    int gender = 0;
    int leader = 0; // 0: 普通成员, 1: 群主, 2: 管理员
    int is_mute = 0; // 是否被禁言
    std::string remark; // 备注
    std::string motto; // 个性签名
    std::string visit_card; // 群名片
};

struct GroupDetail {
    uint64_t group_id = 0;
    std::string group_name;
    std::string profile;
    std::string avatar;
    std::string created_at;
    bool is_manager = false; // 是否是管理员/群主
    int is_disturb = 0; // 是否免打扰
    std::string visit_card; // 我的群名片
    int is_mute = 0; // 全员禁言状态
    int is_overt = 0; // 是否公开
    struct Notice {
        std::string content;
        std::string created_at;
        std::string updated_at;
        std::string modify_user_name;
    } notice;
};

struct GroupOvertItem {
    uint64_t group_id = 0;
    int type = 0;
    std::string name;
    std::string avatar;
    std::string profile;
    int count = 0; // 当前人数
    int max_num = 0; // 最大人数
    bool is_member = false; // 是否已加入
    std::string created_at;
};

struct GroupVoteItem {
    uint64_t vote_id = 0;
    std::string title;
    int answer_mode = 0;  // 0: 单选, 1: 多选
    int is_anonymous = 0; // 是否匿名
    int status = 0;  // 0: 进行中, 1: 已结束
    uint64_t created_by = 0;
    bool is_voted = false; // 是否已投票
    std::string created_at;
};

struct GroupVoteOptionItem {
    uint64_t id = 0;
    std::string content;
    int count = 0; // 票数
    bool is_voted = false; // 当前用户是否投了此项
    std::vector<std::string>
        users;  // 投票用户列表（非匿名时）
};

struct GroupVoteDetail {
    uint64_t vote_id = 0;
    std::string title;
    int answer_mode = 0;
    int is_anonymous = 0;
    int status = 0;
    uint64_t created_by = 0;
    std::string created_at;
    std::vector<GroupVoteOptionItem> options;
    int voted_count = 0; // 总投票人数
    bool is_voted = false;
};

}  // namespace IM::dto
