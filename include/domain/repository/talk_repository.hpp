#ifndef __IM_DOMAIN_REPOSITORY_TALK_REPOSITORY_HPP__
#define __IM_DOMAIN_REPOSITORY_TALK_REPOSITORY_HPP__

#include <memory>
#include <vector>

#include "db/mysql.hpp"
#include "dto/talk_dto.hpp"
#include "model/talk_session.hpp"

namespace IM::domain::repository {

class ITalkRepository {
   public:
    using Ptr = std::shared_ptr<ITalkRepository>;
    virtual ~ITalkRepository() = default;

    // 单聊：根据两个用户ID（自动排序为 user_min_id/user_max_id）查找或创建 talk。
    virtual bool findOrCreateSingleTalk(const std::shared_ptr<IM::MySQL>& db, uint64_t uid1,
                                        uint64_t uid2, uint64_t& out_talk_id,
                                        std::string* err = nullptr) = 0;

    // 群聊：根据 group_id 查找或创建 talk。
    virtual bool findOrCreateGroupTalk(const std::shared_ptr<IM::MySQL>& db, uint64_t group_id,
                                       uint64_t& out_talk_id, std::string* err = nullptr) = 0;

    // 仅查询：获取单聊 talk_id（不存在返回 false 且不写 err 或写入说明）。
    virtual bool getSingleTalkId(const uint64_t uid1, const uint64_t uid2, uint64_t& out_talk_id,
                                 std::string* err = nullptr) = 0;

    // 仅查询：获取群聊 talk_id。
    virtual bool getGroupTalkId(const uint64_t group_id, uint64_t& out_talk_id,
                                std::string* err = nullptr) = 0;

    // 递增并返回新的序列号（从 1 开始）。
    virtual bool nextSeq(const std::shared_ptr<IM::MySQL>& db, uint64_t talk_id, uint64_t& seq,
                         std::string* err = nullptr) = 0;

    // 获取用户的会话列表
    virtual bool getSessionListByUserId(const uint64_t user_id,
                                        std::vector<dto::TalkSessionItem>& out,
                                        std::string* err = nullptr) = 0;

    // 设置会话置顶/取消置顶
    virtual bool setSessionTop(const uint64_t user_id, const uint64_t to_from_id,
                               const uint8_t talk_mode, const uint8_t action,
                               std::string* err = nullptr) = 0;

    // 设置会话免打扰/取消免打扰
    virtual bool setSessionDisturb(const uint64_t user_id, const uint64_t to_from_id,
                                   const uint8_t talk_mode, const uint8_t action,
                                   std::string* err = nullptr) = 0;

    // 创建会话
    virtual bool createSession(const std::shared_ptr<IM::MySQL>& db,
                               const model::TalkSession& session, std::string* err = nullptr) = 0;

    // 获取会话信息
    virtual bool getSessionByUserId(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                                    dto::TalkSessionItem& out, const uint64_t to_from_id,
                                    const uint8_t talk_mode, std::string* err = nullptr) = 0;

    // 删除会话
    virtual bool deleteSession(const uint64_t user_id, const uint64_t to_from_id,
                               const uint8_t talk_mode, std::string* err = nullptr) = 0;

    // 删除会话
    virtual bool deleteSession(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                               const uint64_t to_from_id, const uint8_t talk_mode,
                               std::string* err = nullptr) = 0;

    // 清除会话未读消息数
    virtual bool clearSessionUnreadNum(const uint64_t user_id, const uint64_t to_from_id,
                                       const uint8_t talk_mode, std::string* err = nullptr) = 0;

    // 新消息到达时，推进会话快照与未读数：
    // - 设置 last_msg_id/type/sender/digest/time，updated_at=NOW()
    // - 对除 sender 外的用户未读数 +1（软删除的会话不更新）
    virtual bool bumpOnNewMessage(const std::shared_ptr<IM::MySQL>& db, const uint64_t talk_id,
                                  const uint64_t sender_user_id, const std::string& last_msg_id,
                                  const uint16_t last_msg_type, const std::string& last_msg_digest,
                                  std::string* err = nullptr) = 0;

    // 为指定用户更新会话的最后消息字段（用于用户删除消息后重建摘要）
    // 新增输出参数 `affected`，用于告诉调用方是否有行受影响（更新/清空了 last_msg_* 字段），
    // 若 `affected` 为 false，调用方可选择不进行后续推送/广播等操作，避免无效通知。
    virtual bool updateLastMsgForUser(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                                      const uint64_t talk_id,
                                      const std::optional<std::string>& last_msg_id,
                                      const std::optional<uint16_t>& last_msg_type,
                                      const std::optional<uint64_t>& last_sender_id,
                                      const std::optional<std::string>& last_msg_digest,
                                      std::string* err = nullptr) = 0;

    // 列出对于指定 talk_id 且 last_msg_id 匹配的所有 user_id（用于撤回时重建会话摘要）
    virtual bool listUsersByLastMsg(const std::shared_ptr<IM::MySQL>& db, const uint64_t talk_id,
                                    const std::string& last_msg_id,
                                    std::vector<uint64_t>& out_user_ids,
                                    std::string* err = nullptr) = 0;

    // 列出指定 talk_id 下的所有用户ID（用于广播会话快照更新等）
    virtual bool listUsersByTalkId(const uint64_t talk_id, std::vector<uint64_t>& out_user_ids,
                                   std::string* err = nullptr) = 0;

    // 修改会话备注
    virtual bool editRemarkWithConn(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                                    const uint64_t to_from_id, const std::string& remark,
                                    std::string* err = nullptr) = 0;
   virtual bool updateSessionAvatarByTargetUserWithConn(const std::shared_ptr<IM::MySQL>& db,
                                           const uint64_t target_user_id,
                                           const std::string& avatar,
                                           std::string* err = nullptr) = 0;
   virtual bool listUsersByTargetUserWithConn(const std::shared_ptr<IM::MySQL>& db,
                                    const uint64_t target_user_id,
                                    std::vector<uint64_t>& out_user_ids,
                                    std::string* err = nullptr) = 0;

   // 更新当目标用户（单聊中 to_from_id）更改头像时，更新会话快照中的 avatar 字段
   // 例如用户 A 修改头像后，需要更新所有以 A 为 to_from_id 的单聊会话（其它人的会话视图）
   virtual bool updateSessionAvatarByTargetUser(const uint64_t target_user_id,
                                     const std::string& avatar,
                                     std::string* err = nullptr) = 0;
};

}  // namespace IM::domain::repository

#endif  // __IM_DOMAIN_REPOSITORY_TALK_REPOSITORY_HPP__