#include "infra/repository/group_repository_impl.hpp"

#include "util/time_util.hpp"

namespace IM::infra::repository {

bool GroupRepositoryImpl::CreateGroup(IM::MySQL::ptr conn, model::Group& group, std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "INSERT INTO im_group (group_name, avatar, profile, leader_id, creator_id, created_at, "
        "updated_at) VALUES (?, ?, ?, ?, ?, NOW(), NOW())";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindString(1, group.group_name);
    stmt->bindString(2, group.avatar);
    stmt->bindString(3, group.profile);
    stmt->bindUint64(4, group.leader_id);
    stmt->bindUint64(5, group.creator_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    group.id = stmt->getLastInsertId();
    return true;
}

bool GroupRepositoryImpl::GetGroupById(IM::MySQL::ptr conn, uint64_t group_id, model::Group& group,
                                       std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "SELECT id, group_name, avatar, profile, leader_id, creator_id, is_mute, is_overt, "
        "member_num, is_dismissed, created_at FROM im_group WHERE id = ?";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, group_id);
    auto res = stmt->query();
    if (!res || !res->next()) {
        if (err) *err = "group not found";
        return false;
    }
    group.id = res->getUint64(0);
    group.group_name = res->getString(1);
    group.avatar = res->getString(2);
    group.profile = res->getString(3);
    group.leader_id = res->getUint64(4);
    group.creator_id = res->getUint64(5);
    group.is_mute = res->getInt32(6);
    group.is_overt = res->getInt32(7);
    group.member_num = res->getInt32(8);
    group.is_dismissed = res->getInt32(9);
    group.created_at = res->getString(10);
    return true;
}

bool GroupRepositoryImpl::UpdateGroup(IM::MySQL::ptr conn, const model::Group& group,
                                      std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    std::string sql = "UPDATE im_group SET updated_at = NOW()";
    if (!group.group_name.empty()) sql += ", group_name = ?";
    if (!group.avatar.empty()) sql += ", avatar = ?";
    if (!group.profile.empty()) sql += ", profile = ?";
    if (group.is_mute != 0) sql += ", is_mute = ?";
    if (group.is_overt != 0) sql += ", is_overt = ?";

    sql += " WHERE id = ?";

    auto stmt = conn->prepare(sql.c_str());
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }

    int idx = 1;
    if (!group.group_name.empty()) stmt->bindString(idx++, group.group_name);
    if (!group.avatar.empty()) stmt->bindString(idx++, group.avatar);
    if (!group.profile.empty()) stmt->bindString(idx++, group.profile);
    if (group.is_mute != 0) stmt->bindInt32(idx++, group.is_mute);
    if (group.is_overt != 0) stmt->bindInt32(idx++, group.is_overt);
    stmt->bindUint64(idx++, group.id);

    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool GroupRepositoryImpl::DeleteGroup(IM::MySQL::ptr conn, uint64_t group_id, std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql = "UPDATE im_group SET is_dismissed = 1, dismissed_at = NOW() WHERE id = ?";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, group_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool GroupRepositoryImpl::GetGroupListByUserId(IM::MySQL::ptr conn, uint64_t user_id,
                                               std::vector<dto::GroupItem>& groups,
                                               std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "SELECT g.id, g.group_name, g.avatar, g.profile, g.leader_id, g.creator_id FROM im_group g "
        "JOIN im_group_member m ON g.id = m.group_id WHERE m.user_id = ? AND g.is_dismissed = 0 "
        "AND m.deleted_at IS NULL";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, user_id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    while (res->next()) {
        dto::GroupItem item;
        item.group_id = res->getUint64(0);
        item.group_name = res->getString(1);
        item.avatar = res->getString(2);
        item.profile = res->getString(3);
        item.leader = res->getUint64(4);
        item.creator_id = res->getUint64(5);
        groups.push_back(item);
    }
    return true;
}

bool GroupRepositoryImpl::GetOvertGroupList(IM::MySQL::ptr conn, int page, int size,
                                            const std::string& name,
                                            std::vector<dto::GroupOvertItem>& groups, bool& next,
                                            std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    std::string sql =
        "SELECT id, group_name, avatar, profile, member_num, max_num, created_at FROM im_group "
        "WHERE is_overt = 2 AND is_dismissed = 0";
    if (!name.empty()) {
        sql += " AND group_name LIKE ?";
    }
    sql += " LIMIT ? OFFSET ?";

    auto stmt = conn->prepare(sql.c_str());
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }

    int idx = 1;
    if (!name.empty()) {
        stmt->bindString(idx++, "%" + name + "%");
    }
    stmt->bindInt32(idx++, size + 1);
    stmt->bindInt32(idx++, (page - 1) * size);

    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }

    int count = 0;
    while (res->next()) {
        if (count >= size) {
            next = true;
            break;
        }
        dto::GroupOvertItem item;
        item.group_id = res->getUint64(0);
        item.name = res->getString(1);
        item.avatar = res->getString(2);
        item.profile = res->getString(3);
        item.count = res->getInt32(4);
        item.max_num = res->getInt32(5);
        item.created_at = res->getString(6);
        groups.push_back(item);
        count++;
    }
    return true;
}

bool GroupRepositoryImpl::AddMember(IM::MySQL::ptr conn, const model::GroupMember& member,
                                    std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "INSERT INTO im_group_member (group_id, user_id, role, joined_at, created_at, updated_at) "
        "VALUES (?, ?, ?, NOW(), NOW(), NOW()) ON DUPLICATE KEY UPDATE role = VALUES(role), "
        "deleted_at = NULL, updated_at = NOW()";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, member.group_id);
    stmt->bindUint64(2, member.user_id);
    stmt->bindInt32(3, member.role);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool GroupRepositoryImpl::RemoveMember(IM::MySQL::ptr conn, uint64_t group_id, uint64_t user_id,
                                       std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "UPDATE im_group_member SET deleted_at = NOW() WHERE group_id = ? AND user_id = ?";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, group_id);
    stmt->bindUint64(2, user_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool GroupRepositoryImpl::GetMember(IM::MySQL::ptr conn, uint64_t group_id, uint64_t user_id,
                                    model::GroupMember& member, std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "SELECT id, group_id, user_id, role, visit_card, no_speak_until FROM im_group_member WHERE "
        "group_id = ? AND user_id = ? AND deleted_at IS NULL";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, group_id);
    stmt->bindUint64(2, user_id);
    auto res = stmt->query();
    if (!res || !res->next()) {
        if (err) *err = "member not found";
        return false;
    }
    member.id = res->getUint64(0);
    member.group_id = res->getUint64(1);
    member.user_id = res->getUint64(2);
    member.role = res->getInt32(3);
    member.visit_card = res->getString(4);
    member.no_speak_until = res->getString(5);
    return true;
}

bool GroupRepositoryImpl::GetMemberList(IM::MySQL::ptr conn, uint64_t group_id,
                                        std::vector<dto::GroupMemberItem>& members,
                                        std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "SELECT m.user_id, u.nickname, u.avatar, u.gender, m.role, m.visit_card, u.motto FROM "
        "im_group_member m JOIN im_user u ON m.user_id = u.id WHERE m.group_id = ? AND "
        "m.deleted_at IS NULL";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, group_id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    while (res->next()) {
        dto::GroupMemberItem item;
        item.user_id = res->getUint64(0);
        item.nickname = res->getString(1);
        item.avatar = res->getString(2);
        item.gender = res->getInt32(3);
        item.leader = res->getInt32(4);  // role
        item.visit_card = res->getString(5);
        item.motto = res->getString(6);
        members.push_back(item);
    }
    return true;
}

bool GroupRepositoryImpl::UpdateMemberRole(IM::MySQL::ptr conn, uint64_t group_id, uint64_t user_id,
                                           int role, std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql = "UPDATE im_group_member SET role = ? WHERE group_id = ? AND user_id = ?";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindInt32(1, role);
    stmt->bindUint64(2, group_id);
    stmt->bindUint64(3, user_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool GroupRepositoryImpl::UpdateMemberMute(IM::MySQL::ptr conn, uint64_t group_id, uint64_t user_id,
                                           const std::string& until, std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "UPDATE im_group_member SET no_speak_until = ? WHERE group_id = ? AND user_id = ?";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    if (until.empty()) {
        stmt->bindNull(1);
    } else {
        stmt->bindString(1, until);
    }
    stmt->bindUint64(2, group_id);
    stmt->bindUint64(3, user_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool GroupRepositoryImpl::GetMemberCount(IM::MySQL::ptr conn, uint64_t group_id, int& count,
                                         std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "SELECT COUNT(*) FROM im_group_member WHERE group_id = ? AND deleted_at IS NULL";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, group_id);
    auto res = stmt->query();
    if (!res || !res->next()) {
        if (err) *err = "query failed";
        return false;
    }
    count = res->getInt32(0);
    return true;
}

bool GroupRepositoryImpl::CreateApply(IM::MySQL::ptr conn, const model::GroupApply& apply,
                                      std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "INSERT INTO im_group_apply (group_id, user_id, remark, status, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, NOW(), NOW())";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, apply.group_id);
    stmt->bindUint64(2, apply.user_id);
    stmt->bindString(3, apply.remark);
    stmt->bindInt32(4, apply.status);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool GroupRepositoryImpl::GetApplyById(IM::MySQL::ptr conn, uint64_t apply_id,
                                       model::GroupApply& apply, std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "SELECT id, group_id, user_id, remark, status FROM im_group_apply WHERE id = ?";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, apply_id);
    auto res = stmt->query();
    if (!res || !res->next()) {
        if (err) *err = "apply not found";
        return false;
    }
    apply.id = res->getUint64(0);
    apply.group_id = res->getUint64(1);
    apply.user_id = res->getUint64(2);
    apply.remark = res->getString(3);
    apply.status = res->getInt32(4);
    return true;
}

bool GroupRepositoryImpl::UpdateApplyStatus(IM::MySQL::ptr conn, uint64_t apply_id, int status,
                                            uint64_t handler_id, std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "UPDATE im_group_apply SET status = ?, handler_user_id = ?, handled_at = NOW(), updated_at "
        "= NOW() WHERE id = ?";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindInt32(1, status);
    stmt->bindUint64(2, handler_id);
    stmt->bindUint64(3, apply_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool GroupRepositoryImpl::GetApplyList(IM::MySQL::ptr conn, uint64_t group_id,
                                       std::vector<dto::GroupApplyItem>& applies,
                                       std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "SELECT a.id, a.user_id, a.group_id, a.remark, u.nickname, u.avatar, a.created_at FROM "
        "im_group_apply a JOIN im_user u ON a.user_id = u.id WHERE a.group_id = ? AND a.status = 1";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, group_id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    while (res->next()) {
        dto::GroupApplyItem item;
        item.id = res->getUint64(0);
        item.user_id = res->getUint64(1);
        item.group_id = res->getUint64(2);
        item.remark = res->getString(3);
        item.nickname = res->getString(4);
        item.avatar = res->getString(5);
        item.created_at = res->getString(6);
        applies.push_back(item);
    }
    return true;
}

bool GroupRepositoryImpl::GetUserApplyList(IM::MySQL::ptr conn, uint64_t user_id,
                                           std::vector<dto::GroupApplyItem>& applies,
                                           std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "SELECT a.id, a.user_id, a.group_id, a.remark, g.group_name, g.avatar, a.created_at FROM "
        "im_group_apply a JOIN im_group g ON a.group_id = g.id WHERE a.user_id = ?";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, user_id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    while (res->next()) {
        dto::GroupApplyItem item;
        item.id = res->getUint64(0);
        item.user_id = res->getUint64(1);
        item.group_id = res->getUint64(2);
        item.remark = res->getString(3);
        item.group_name = res->getString(4);
        item.avatar = res->getString(5);
        item.created_at = res->getString(6);
        applies.push_back(item);
    }
    return true;
}

bool GroupRepositoryImpl::GetUnreadApplyCount(IM::MySQL::ptr conn, uint64_t user_id, int& count,
                                              std::string* err) {
    // This is tricky. User needs to be admin/owner of groups to see unread applies.
    // So we need to join group_member to find groups where user is admin/owner, then count applies for those groups.
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "SELECT COUNT(*) FROM im_group_apply a JOIN im_group_member m ON a.group_id = m.group_id "
        "WHERE m.user_id = ? AND (m.role = 2 OR m.role = 3) AND a.status = 1 AND a.is_read = 0";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, user_id);
    auto res = stmt->query();
    if (!res || !res->next()) {
        if (err) *err = "query failed";
        return false;
    }
    count = res->getInt32(0);
    return true;
}

bool GroupRepositoryImpl::UpdateNotice(IM::MySQL::ptr conn, const model::GroupNotice& notice,
                                       std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "INSERT INTO im_group_notice (group_id, content, modify_user_id, created_at, updated_at) "
        "VALUES (?, ?, ?, NOW(), NOW()) ON DUPLICATE KEY UPDATE content = VALUES(content), "
        "modify_user_id = VALUES(modify_user_id), updated_at = NOW()";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, notice.group_id);
    stmt->bindString(2, notice.content);
    stmt->bindUint64(3, notice.modify_user_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool GroupRepositoryImpl::GetNotice(IM::MySQL::ptr conn, uint64_t group_id,
                                    model::GroupNotice& notice, std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "SELECT content, modify_user_id, created_at, updated_at FROM im_group_notice WHERE "
        "group_id = ?";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, group_id);
    auto res = stmt->query();
    if (!res || !res->next()) {
        // Notice might not exist, which is fine.
        return false;
    }
    notice.group_id = group_id;
    notice.content = res->getString(0);
    notice.modify_user_id = res->getUint64(1);
    notice.created_at = res->getString(2);
    notice.updated_at = res->getString(3);
    return true;
}

bool GroupRepositoryImpl::CreateVote(IM::MySQL::ptr conn, model::GroupVote& vote,
                                     const std::vector<model::GroupVoteOption>& options,
                                     std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }

    // Insert Vote
    const char* sql =
        "INSERT INTO im_group_vote (group_id, title, answer_mode, is_anonymous, created_by, "
        "deadline_at, status, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, NOW(), NOW())";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, vote.group_id);
    stmt->bindString(2, vote.title);
    stmt->bindInt32(3, vote.answer_mode);
    stmt->bindInt32(4, vote.is_anonymous);
    stmt->bindUint64(5, vote.created_by);
    if (vote.deadline_at.empty()) {
        stmt->bindNull(6);
    } else {
        stmt->bindString(6, vote.deadline_at);
    }
    stmt->bindInt32(7, vote.status);

    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    vote.id = stmt->getLastInsertId();

    // Insert Options
    if (!options.empty()) {
        std::string opt_sql =
            "INSERT INTO im_group_vote_option (vote_id, opt_key, opt_value, sort) VALUES ";
        for (size_t i = 0; i < options.size(); ++i) {
            if (i > 0) opt_sql += ", ";
            opt_sql += "(?, ?, ?, ?)";
        }
        auto opt_stmt = conn->prepare(opt_sql.c_str());
        if (!opt_stmt) {
            if (err) *err = conn->getErrStr();
            return false;
        }
        int idx = 1;
        for (const auto& opt : options) {
            opt_stmt->bindUint64(idx++, vote.id);
            opt_stmt->bindString(idx++, opt.opt_key);
            opt_stmt->bindString(idx++, opt.opt_value);
            opt_stmt->bindInt32(idx++, opt.sort);
        }
        if (opt_stmt->execute() != 0) {
            if (err) *err = opt_stmt->getErrStr();
            return false;
        }
    }
    return true;
}

bool GroupRepositoryImpl::GetVoteList(IM::MySQL::ptr conn, uint64_t group_id,
                                      std::vector<model::GroupVote>& votes, std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "SELECT id, group_id, title, answer_mode, is_anonymous, created_by, deadline_at, status, "
        "created_at FROM im_group_vote WHERE group_id = ? ORDER BY created_at DESC";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, group_id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    while (res->next()) {
        model::GroupVote vote;
        vote.id = res->getUint64(0);
        vote.group_id = res->getUint64(1);
        vote.title = res->getString(2);
        vote.answer_mode = res->getInt32(3);
        vote.is_anonymous = res->getInt32(4);
        vote.created_by = res->getUint64(5);
        if (!res->isNull(6)) vote.deadline_at = res->getString(6);
        vote.status = res->getInt32(7);
        vote.created_at = res->getString(8);
        votes.push_back(vote);
    }
    return true;
}

bool GroupRepositoryImpl::GetVote(IM::MySQL::ptr conn, uint64_t vote_id, model::GroupVote& vote,
                                  std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "SELECT id, group_id, title, answer_mode, is_anonymous, created_by, deadline_at, status, "
        "created_at FROM im_group_vote WHERE id = ?";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, vote_id);
    auto res = stmt->query();
    if (!res || !res->next()) {
        if (err) *err = "vote not found";
        return false;
    }
    vote.id = res->getUint64(0);
    vote.group_id = res->getUint64(1);
    vote.title = res->getString(2);
    vote.answer_mode = res->getInt32(3);
    vote.is_anonymous = res->getInt32(4);
    vote.created_by = res->getUint64(5);
    if (!res->isNull(6)) vote.deadline_at = res->getString(6);
    vote.status = res->getInt32(7);
    vote.created_at = res->getString(8);
    return true;
}

bool GroupRepositoryImpl::GetVoteOptions(IM::MySQL::ptr conn, uint64_t vote_id,
                                         std::vector<model::GroupVoteOption>& options,
                                         std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "SELECT id, vote_id, opt_key, opt_value, sort FROM im_group_vote_option WHERE vote_id = ? "
        "ORDER BY sort ASC";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, vote_id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    while (res->next()) {
        model::GroupVoteOption opt;
        opt.id = res->getUint64(0);
        opt.vote_id = res->getUint64(1);
        opt.opt_key = res->getString(2);
        opt.opt_value = res->getString(3);
        opt.sort = res->getInt32(4);
        options.push_back(opt);
    }
    return true;
}

bool GroupRepositoryImpl::GetVoteAnswers(IM::MySQL::ptr conn, uint64_t vote_id,
                                         std::vector<model::GroupVoteAnswer>& answers,
                                         std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "SELECT vote_id, user_id, opt_key, answered_at FROM im_group_vote_answer WHERE vote_id = ?";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, vote_id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    while (res->next()) {
        model::GroupVoteAnswer ans;
        ans.vote_id = res->getUint64(0);
        ans.user_id = res->getUint64(1);
        ans.opt_key = res->getString(2);
        ans.answered_at = res->getString(3);
        answers.push_back(ans);
    }
    return true;
}

bool GroupRepositoryImpl::CastVote(IM::MySQL::ptr conn, const model::GroupVoteAnswer& answer,
                                   std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "INSERT INTO im_group_vote_answer (vote_id, user_id, opt_key, answered_at) VALUES (?, ?, "
        "?, NOW())";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, answer.vote_id);
    stmt->bindUint64(2, answer.user_id);
    stmt->bindString(3, answer.opt_key);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool GroupRepositoryImpl::FinishVote(IM::MySQL::ptr conn, uint64_t vote_id, std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql = "UPDATE im_group_vote SET status = 2, updated_at = NOW() WHERE id = ?";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, vote_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool GroupRepositoryImpl::GetVoteAnsweredUserIds(IM::MySQL::ptr conn, uint64_t vote_id,
                                                 std::vector<uint64_t>& user_ids,
                                                 std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql = "SELECT DISTINCT user_id FROM im_group_vote_answer WHERE vote_id = ?";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, vote_id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    while (res->next()) {
        user_ids.push_back(res->getUint64(0));
    }
    return true;
}

}  // namespace IM::infra::repository
