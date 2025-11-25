#pragma once
#include <memory>
#include <vector>
#include "base/macro.hpp"
#include "db/mysql.hpp"
#include "dto/group_dto.hpp"
#include "model/group.hpp"
#include "model/group_apply.hpp"
#include "model/group_member.hpp"
#include "model/group_notice.hpp"
#include "model/group_vote.hpp"

namespace IM::domain::repository {

class IGroupRepository {
public:
    using Ptr = std::shared_ptr<IGroupRepository>;
    virtual ~IGroupRepository() = default;

    // Group
    // 创建群组
    virtual bool CreateGroup(IM::MySQL::ptr conn, model::Group& group, std::string* err) = 0;
    // 根据ID获取群组信息
    virtual bool GetGroupById(IM::MySQL::ptr conn, uint64_t group_id, model::Group& group, std::string* err) = 0;
    // 更新群组信息
    virtual bool UpdateGroup(IM::MySQL::ptr conn, const model::Group& group, std::string* err) = 0;
    // 删除群组（解散）
    virtual bool DeleteGroup(IM::MySQL::ptr conn, uint64_t group_id, std::string* err) = 0;
    // 获取用户的群组列表
    virtual bool GetGroupListByUserId(IM::MySQL::ptr conn, uint64_t user_id, std::vector<dto::GroupItem>& groups, std::string* err) = 0;
    // 获取公开群组列表
    virtual bool GetOvertGroupList(IM::MySQL::ptr conn, int page, int size, const std::string& name, std::vector<dto::GroupOvertItem>& groups, bool& next, std::string* err) = 0;

    // Member
    // 添加群成员
    virtual bool AddMember(IM::MySQL::ptr conn, const model::GroupMember& member, std::string* err) = 0;
    // 移除群成员
    virtual bool RemoveMember(IM::MySQL::ptr conn, uint64_t group_id, uint64_t user_id, std::string* err) = 0;
    // 获取群成员信息
    virtual bool GetMember(IM::MySQL::ptr conn, uint64_t group_id, uint64_t user_id, model::GroupMember& member, std::string* err) = 0;
    // 获取群成员列表
    virtual bool GetMemberList(IM::MySQL::ptr conn, uint64_t group_id, std::vector<dto::GroupMemberItem>& members, std::string* err) = 0;
    // 更新成员角色
    virtual bool UpdateMemberRole(IM::MySQL::ptr conn, uint64_t group_id, uint64_t user_id, int role, std::string* err) = 0;
    // 更新成员禁言状态
    virtual bool UpdateMemberMute(IM::MySQL::ptr conn, uint64_t group_id, uint64_t user_id, const std::string& until, std::string* err) = 0;
    // 获取群成员数量
    virtual bool GetMemberCount(IM::MySQL::ptr conn, uint64_t group_id, int& count, std::string* err) = 0;

    // Apply
    // 创建入群申请
    virtual bool CreateApply(IM::MySQL::ptr conn, const model::GroupApply& apply, std::string* err) = 0;
    // 获取申请详情
    virtual bool GetApplyById(IM::MySQL::ptr conn, uint64_t apply_id, model::GroupApply& apply, std::string* err) = 0;
    // 更新申请状态
    virtual bool UpdateApplyStatus(IM::MySQL::ptr conn, uint64_t apply_id, int status, uint64_t handler_id, std::string* err) = 0;
    // 获取群组的申请列表
    virtual bool GetApplyList(IM::MySQL::ptr conn, uint64_t group_id, std::vector<dto::GroupApplyItem>& applies, std::string* err) = 0;
    // 获取用户的申请列表
    virtual bool GetUserApplyList(IM::MySQL::ptr conn, uint64_t user_id, std::vector<dto::GroupApplyItem>& applies, std::string* err) = 0;
    // 获取未读申请数量
    virtual bool GetUnreadApplyCount(IM::MySQL::ptr conn, uint64_t user_id, int& count, std::string* err) = 0;

    // Notice
    // 更新群公告
    virtual bool UpdateNotice(IM::MySQL::ptr conn, const model::GroupNotice& notice, std::string* err) = 0;
    // 获取群公告
    virtual bool GetNotice(IM::MySQL::ptr conn, uint64_t group_id, model::GroupNotice& notice, std::string* err) = 0;

    // Vote
    // 创建投票
    virtual bool CreateVote(IM::MySQL::ptr conn, model::GroupVote& vote, const std::vector<model::GroupVoteOption>& options, std::string* err) = 0;
    // 获取群投票列表
    virtual bool GetVoteList(IM::MySQL::ptr conn, uint64_t group_id, std::vector<model::GroupVote>& votes, std::string* err) = 0;
    // 获取投票详情
    virtual bool GetVote(IM::MySQL::ptr conn, uint64_t vote_id, model::GroupVote& vote, std::string* err) = 0;
    // 获取投票选项
    virtual bool GetVoteOptions(IM::MySQL::ptr conn, uint64_t vote_id, std::vector<model::GroupVoteOption>& options, std::string* err) = 0;
    // 获取投票答案
    virtual bool GetVoteAnswers(IM::MySQL::ptr conn, uint64_t vote_id, std::vector<model::GroupVoteAnswer>& answers, std::string* err) = 0;
    // 提交投票
    virtual bool CastVote(IM::MySQL::ptr conn, const model::GroupVoteAnswer& answer, std::string* err) = 0;
    // 结束投票
    virtual bool FinishVote(IM::MySQL::ptr conn, uint64_t vote_id, std::string* err) = 0;
    // 获取已投票用户ID列表
    virtual bool GetVoteAnsweredUserIds(IM::MySQL::ptr conn, uint64_t vote_id, std::vector<uint64_t>& user_ids, std::string* err) = 0;
};

}
