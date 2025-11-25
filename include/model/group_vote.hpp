#pragma once
#include <string>
#include <vector>

#include "base/macro.hpp"

namespace IM::model {

struct GroupVote {
    uint64_t id = 0;
    uint64_t group_id = 0;
    std::string title;           // 投票标题
    int answer_mode = 1;         // 答题模式 0:单选 1:多选
    int is_anonymous = 0;        // 是否匿名 0:否 1:是
    uint64_t created_by = 0;     // 创建人ID
    std::string deadline_at;     // 截止时间
    int status = 1;              // 状态 0:进行中 1:已结束
    std::string created_at;      // 创建时间
    std::string updated_at;      // 更新时间
};

struct GroupVoteOption {
    uint64_t id = 0;
    uint64_t vote_id = 0;
    std::string opt_key;         // 选项Key
    std::string opt_value;       // 选项内容
    int sort = 0;                // 排序
};

struct GroupVoteAnswer {
    uint64_t vote_id = 0;
    uint64_t user_id = 0;
    std::string opt_key;         // 选项Key
    std::string answered_at;     // 投票时间
};

}  // namespace IM::model
