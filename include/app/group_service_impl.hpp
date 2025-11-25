#pragma once
#include "domain/service/group_service.hpp"
#include "domain/repository/group_repository.hpp"
#include "domain/repository/user_repository.hpp"
#include "domain/service/message_service.hpp"
#include "domain/service/talk_service.hpp"

namespace IM::app {

class GroupServiceImpl : public domain::service::IGroupService {
public:
    GroupServiceImpl(domain::repository::IGroupRepository::Ptr group_repo,
                     domain::repository::IUserRepository::Ptr user_repo,
                     domain::service::IMessageService::Ptr message_service,
                     domain::service::ITalkService::Ptr talk_service);
    ~GroupServiceImpl() override = default;

    // Group
    Result<uint64_t> CreateGroup(uint64_t user_id, const std::string& name, const std::vector<uint64_t>& member_ids) override;
    Result<void> DismissGroup(uint64_t user_id, uint64_t group_id) override;
    Result<dto::GroupDetail> GetGroupDetail(uint64_t user_id, uint64_t group_id) override;
    Result<std::vector<dto::GroupItem>> GetGroupList(uint64_t user_id) override;
    Result<void> UpdateGroupSetting(uint64_t user_id, uint64_t group_id, const std::string& name, const std::string& avatar, const std::string& profile) override;
    Result<void> HandoverGroup(uint64_t user_id, uint64_t group_id, uint64_t new_owner_id) override;
    Result<void> AssignAdmin(uint64_t user_id, uint64_t group_id, uint64_t target_id, int action) override;
    Result<void> MuteGroup(uint64_t user_id, uint64_t group_id, int action) override;
    Result<void> OvertGroup(uint64_t user_id, uint64_t group_id, int action) override;
    Result<std::pair<std::vector<dto::GroupOvertItem>, bool>> GetOvertGroupList(int page, const std::string& name) override;

    // Member
    Result<std::vector<dto::GroupMemberItem>> GetGroupMemberList(uint64_t user_id, uint64_t group_id) override;
    Result<void> InviteGroup(uint64_t user_id, uint64_t group_id, const std::vector<uint64_t>& member_ids) override;
    Result<void> RemoveMember(uint64_t user_id, uint64_t group_id, const std::vector<uint64_t>& member_ids) override;
    Result<void> SecedeGroup(uint64_t user_id, uint64_t group_id) override;
    Result<void> UpdateMemberRemark(uint64_t user_id, uint64_t group_id, const std::string& remark) override;
    Result<void> MuteMember(uint64_t user_id, uint64_t group_id, uint64_t target_id, int action) override;

    // Apply
    Result<void> CreateApply(uint64_t user_id, uint64_t group_id, const std::string& remark) override;
    Result<void> AgreeApply(uint64_t user_id, uint64_t apply_id) override;
    Result<void> DeclineApply(uint64_t user_id, uint64_t apply_id, const std::string& remark) override;
    Result<std::vector<dto::GroupApplyItem>> GetApplyList(uint64_t user_id, uint64_t group_id) override;
    Result<std::vector<dto::GroupApplyItem>> GetUserApplyList(uint64_t user_id) override;
    Result<int> GetUnreadApplyCount(uint64_t user_id) override;

    // Notice
    Result<void> EditNotice(uint64_t user_id, uint64_t group_id, const std::string& content) override;

    // Vote
    Result<uint64_t> CreateVote(uint64_t user_id, uint64_t group_id, const std::string& title, int answer_mode, int is_anonymous, const std::vector<std::string>& options) override;
    Result<std::vector<dto::GroupVoteItem>> GetVoteList(uint64_t user_id, uint64_t group_id) override;
    Result<dto::GroupVoteDetail> GetVoteDetail(uint64_t user_id, uint64_t vote_id) override;
    Result<void> CastVote(uint64_t user_id, uint64_t vote_id, const std::vector<std::string>& options) override;
    Result<void> FinishVote(uint64_t user_id, uint64_t vote_id) override;

private:
    domain::repository::IGroupRepository::Ptr m_group_repo;
    domain::repository::IUserRepository::Ptr m_user_repo;
    domain::service::IMessageService::Ptr m_message_service;
    domain::service::ITalkService::Ptr m_talk_service;
};

}
