#pragma once
#include "domain/repository/group_repository.hpp"

namespace IM::infra::repository {

class GroupRepositoryImpl : public domain::repository::IGroupRepository {
public:
    using Ptr = std::shared_ptr<GroupRepositoryImpl>;

    // Group
    bool CreateGroup(IM::MySQL::ptr conn, model::Group& group, std::string* err) override;
    bool GetGroupById(IM::MySQL::ptr conn, uint64_t group_id, model::Group& group, std::string* err) override;
    bool UpdateGroup(IM::MySQL::ptr conn, const model::Group& group, std::string* err) override;
    bool DeleteGroup(IM::MySQL::ptr conn, uint64_t group_id, std::string* err) override;
    bool GetGroupListByUserId(IM::MySQL::ptr conn, uint64_t user_id, std::vector<dto::GroupItem>& groups, std::string* err) override;
    bool GetOvertGroupList(IM::MySQL::ptr conn, int page, int size, const std::string& name, std::vector<dto::GroupOvertItem>& groups, bool& next, std::string* err) override;

    // Member
    bool AddMember(IM::MySQL::ptr conn, const model::GroupMember& member, std::string* err) override;
    bool RemoveMember(IM::MySQL::ptr conn, uint64_t group_id, uint64_t user_id, std::string* err) override;
    bool GetMember(IM::MySQL::ptr conn, uint64_t group_id, uint64_t user_id, model::GroupMember& member, std::string* err) override;
    bool GetMemberList(IM::MySQL::ptr conn, uint64_t group_id, std::vector<dto::GroupMemberItem>& members, std::string* err) override;
    bool UpdateMemberRole(IM::MySQL::ptr conn, uint64_t group_id, uint64_t user_id, int role, std::string* err) override;
    bool UpdateMemberMute(IM::MySQL::ptr conn, uint64_t group_id, uint64_t user_id, const std::string& until, std::string* err) override;
    bool GetMemberCount(IM::MySQL::ptr conn, uint64_t group_id, int& count, std::string* err) override;

    // Apply
    bool CreateApply(IM::MySQL::ptr conn, const model::GroupApply& apply, std::string* err) override;
    bool GetApplyById(IM::MySQL::ptr conn, uint64_t apply_id, model::GroupApply& apply, std::string* err) override;
    bool UpdateApplyStatus(IM::MySQL::ptr conn, uint64_t apply_id, int status, uint64_t handler_id, std::string* err) override;
    bool GetApplyList(IM::MySQL::ptr conn, uint64_t group_id, std::vector<dto::GroupApplyItem>& applies, std::string* err) override;
    bool GetUserApplyList(IM::MySQL::ptr conn, uint64_t user_id, std::vector<dto::GroupApplyItem>& applies, std::string* err) override;
    bool GetUnreadApplyCount(IM::MySQL::ptr conn, uint64_t user_id, int& count, std::string* err) override;

    // Notice
    bool UpdateNotice(IM::MySQL::ptr conn, const model::GroupNotice& notice, std::string* err) override;
    bool GetNotice(IM::MySQL::ptr conn, uint64_t group_id, model::GroupNotice& notice, std::string* err) override;

    // Vote
    bool CreateVote(IM::MySQL::ptr conn, model::GroupVote& vote, const std::vector<model::GroupVoteOption>& options, std::string* err) override;
    bool GetVoteList(IM::MySQL::ptr conn, uint64_t group_id, std::vector<model::GroupVote>& votes, std::string* err) override;
    bool GetVote(IM::MySQL::ptr conn, uint64_t vote_id, model::GroupVote& vote, std::string* err) override;
    bool GetVoteOptions(IM::MySQL::ptr conn, uint64_t vote_id, std::vector<model::GroupVoteOption>& options, std::string* err) override;
    bool GetVoteAnswers(IM::MySQL::ptr conn, uint64_t vote_id, std::vector<model::GroupVoteAnswer>& answers, std::string* err) override;
    bool CastVote(IM::MySQL::ptr conn, const model::GroupVoteAnswer& answer, std::string* err) override;
    bool FinishVote(IM::MySQL::ptr conn, uint64_t vote_id, std::string* err) override;
    bool GetVoteAnsweredUserIds(IM::MySQL::ptr conn, uint64_t vote_id, std::vector<uint64_t>& user_ids, std::string* err) override;
};

}
