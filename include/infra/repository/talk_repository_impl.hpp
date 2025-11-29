#ifndef __IM_INFRA_REPOSITORY_TALK_REPOSITORY_IMPL_HPP__
#define __IM_INFRA_REPOSITORY_TALK_REPOSITORY_IMPL_HPP__

#include "db/mysql.hpp"
#include "domain/repository/talk_repository.hpp"
#include "dto/contact_dto.hpp"

namespace IM::infra::repository {

class TalkRepositoryImpl : public IM::domain::repository::ITalkRepository {
   public:
    explicit TalkRepositoryImpl(std::shared_ptr<IM::MySQLManager> db_manager);

    bool findOrCreateSingleTalk(const std::shared_ptr<IM::MySQL>& db, uint64_t uid1, uint64_t uid2,
                                uint64_t& out_talk_id, std::string* err = nullptr) override;
    bool findOrCreateGroupTalk(const std::shared_ptr<IM::MySQL>& db, uint64_t group_id,
                               uint64_t& out_talk_id, std::string* err = nullptr) override;
    bool getSingleTalkId(const uint64_t uid1, const uint64_t uid2, uint64_t& out_talk_id,
                         std::string* err = nullptr) override;
    bool getGroupTalkId(const uint64_t group_id, uint64_t& out_talk_id,
                        std::string* err = nullptr) override;
    bool nextSeq(const std::shared_ptr<IM::MySQL>& db, uint64_t talk_id, uint64_t& seq,
                 std::string* err = nullptr) override;
    bool getSessionListByUserId(const uint64_t user_id, std::vector<dto::TalkSessionItem>& out,
                                std::string* err = nullptr) override;
    bool setSessionTop(const uint64_t user_id, const uint64_t to_from_id, const uint8_t talk_mode,
                       const uint8_t action, std::string* err = nullptr) override;
    bool setSessionDisturb(const uint64_t user_id, const uint64_t to_from_id,
                           const uint8_t talk_mode, const uint8_t action,
                           std::string* err = nullptr) override;
    bool createSession(const std::shared_ptr<IM::MySQL>& db, const model::TalkSession& session,
                       std::string* err = nullptr) override;
    bool getSessionByUserId(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                            dto::TalkSessionItem& out, const uint64_t to_from_id,
                            const uint8_t talk_mode, std::string* err = nullptr) override;
    bool deleteSession(const uint64_t user_id, const uint64_t to_from_id, const uint8_t talk_mode,
                       std::string* err = nullptr) override;
    bool deleteSession(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                       const uint64_t to_from_id, const uint8_t talk_mode,
                       std::string* err = nullptr) override;
    bool clearSessionUnreadNum(const uint64_t user_id, const uint64_t to_from_id,
                               const uint8_t talk_mode, std::string* err = nullptr) override;
    bool bumpOnNewMessage(const std::shared_ptr<IM::MySQL>& db, const uint64_t talk_id,
                          const uint64_t sender_user_id, const std::string& last_msg_id,
                          const uint16_t last_msg_type, const std::string& last_msg_digest,
                          std::string* err = nullptr) override;
    bool updateLastMsgForUser(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                              const uint64_t talk_id, const std::optional<std::string>& last_msg_id,
                              const std::optional<uint16_t>& last_msg_type,
                              const std::optional<uint64_t>& last_sender_id,
                              const std::optional<std::string>& last_msg_digest,
                              std::string* err = nullptr) override;
    bool listUsersByLastMsg(const std::shared_ptr<IM::MySQL>& db, const uint64_t talk_id,
                            const std::string& last_msg_id, std::vector<uint64_t>& out_user_ids,
                            std::string* err = nullptr) override;
    bool listUsersByTalkId(const uint64_t talk_id, std::vector<uint64_t>& out_user_ids,
                           std::string* err = nullptr) override;
    bool updateSessionAvatarByTargetUserWithConn(const std::shared_ptr<IM::MySQL>& db,
                                                const uint64_t target_user_id,
                                                const std::string& avatar,
                                                std::string* err = nullptr) override;
    bool listUsersByTargetUserWithConn(const std::shared_ptr<IM::MySQL>& db,
                                      const uint64_t target_user_id,
                                      std::vector<uint64_t>& out_user_ids,
                                      std::string* err = nullptr) override;
    bool editRemarkWithConn(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                            const uint64_t to_from_id, const std::string& remark,
                            std::string* err = nullptr) override;
    bool updateSessionAvatarByTargetUser(const uint64_t target_user_id,
                                        const std::string& avatar,
                                        std::string* err = nullptr) override;

   private:
    std::shared_ptr<IM::MySQLManager> m_db_manager;
};

}  // namespace IM::infra::repository

#endif  // __IM_INFRA_REPOSITORY_TALK_REPOSITORY_IMPL_HPP__