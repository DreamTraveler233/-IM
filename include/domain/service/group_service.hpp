#pragma once
#include <memory>
#include <vector>
#include "base/macro.hpp"
#include "common/common.hpp"
#include "dto/group_dto.hpp"

namespace IM::domain::service {

class IGroupService {
public:
    using Ptr = std::shared_ptr<IGroupService>;
    virtual ~IGroupService() = default;

    // Group
    // 创建群组
    virtual Result<uint64_t> CreateGroup(uint64_t user_id, const std::string& name, const std::vector<uint64_t>& member_ids) = 0;
    // 解散群组
    virtual Result<void> DismissGroup(uint64_t user_id, uint64_t group_id) = 0;
    // 获取群组详情
    virtual Result<dto::GroupDetail> GetGroupDetail(uint64_t user_id, uint64_t group_id) = 0;
    // 获取用户的群组列表
    virtual Result<std::vector<dto::GroupItem>> GetGroupList(uint64_t user_id) = 0;
    // 更新群组设置（名称、头像、简介）
    virtual Result<void> UpdateGroupSetting(uint64_t user_id, uint64_t group_id, const std::string& name, const std::string& avatar, const std::string& profile) = 0;
    // 转让群主
    virtual Result<void> HandoverGroup(uint64_t user_id, uint64_t group_id, uint64_t new_owner_id) = 0;
    // 设置/取消管理员
    virtual Result<void> AssignAdmin(uint64_t user_id, uint64_t group_id, uint64_t target_id, int action) = 0;
    // 全员禁言/解除
    virtual Result<void> MuteGroup(uint64_t user_id, uint64_t group_id, int action) = 0;
    // 公开/隐藏群组
    virtual Result<void> OvertGroup(uint64_t user_id, uint64_t group_id, int action) = 0;
    // 获取公开群组列表
    virtual Result<std::pair<std::vector<dto::GroupOvertItem>, bool>> GetOvertGroupList(int page, const std::string& name) = 0;

    // Member
    // 获取群成员列表
    virtual Result<std::vector<dto::GroupMemberItem>> GetGroupMemberList(uint64_t user_id, uint64_t group_id) = 0;
    // 邀请成员入群
    virtual Result<void> InviteGroup(uint64_t user_id, uint64_t group_id, const std::vector<uint64_t>& member_ids) = 0;
    // 移除群成员（踢人）
    virtual Result<void> RemoveMember(uint64_t user_id, uint64_t group_id, const std::vector<uint64_t>& member_ids) = 0;
    // 退出群组
    virtual Result<void> SecedeGroup(uint64_t user_id, uint64_t group_id) = 0;
    // 更新成员备注
    virtual Result<void> UpdateMemberRemark(uint64_t user_id, uint64_t group_id, const std::string& remark) = 0;
    // 禁言/解除成员
    virtual Result<void> MuteMember(uint64_t user_id, uint64_t group_id, uint64_t target_id, int action) = 0; 

    // Apply
    // 申请入群
    virtual Result<void> CreateApply(uint64_t user_id, uint64_t group_id, const std::string& remark) = 0;
    // 同意入群申请
    virtual Result<void> AgreeApply(uint64_t user_id, uint64_t apply_id) = 0;
    // 拒绝入群申请
    virtual Result<void> DeclineApply(uint64_t user_id, uint64_t apply_id, const std::string& remark) = 0;
    // 获取群组的申请列表（管理员/群主）
    virtual Result<std::vector<dto::GroupApplyItem>> GetApplyList(uint64_t user_id, uint64_t group_id) = 0; 
    // 获取用户的申请列表
    virtual Result<std::vector<dto::GroupApplyItem>> GetUserApplyList(uint64_t user_id) = 0; 
    // 获取未读申请数量
    virtual Result<int> GetUnreadApplyCount(uint64_t user_id) = 0;

    // Vote
    // 发起投票
    virtual Result<uint64_t> CreateVote(uint64_t user_id, uint64_t group_id, const std::string& title, int answer_mode, int is_anonymous, const std::vector<std::string>& options) = 0;
    // 获取群投票列表
    virtual Result<std::vector<dto::GroupVoteItem>> GetVoteList(uint64_t user_id, uint64_t group_id) = 0;
    // 获取投票详情
    virtual Result<dto::GroupVoteDetail> GetVoteDetail(uint64_t user_id, uint64_t vote_id) = 0;
    // 参与投票
    virtual Result<void> CastVote(uint64_t user_id, uint64_t vote_id, const std::vector<std::string>& options) = 0;
    // 结束投票
    virtual Result<void> FinishVote(uint64_t user_id, uint64_t vote_id) = 0;

    // Notice
    // 编辑/发布群公告
    virtual Result<void> EditNotice(uint64_t user_id, uint64_t group_id, const std::string& content) = 0;
};

}
