#ifndef __IM_APP_TALK_SERVICE_IMPL_HPP__
#define __IM_APP_TALK_SERVICE_IMPL_HPP__

#include "domain/repository/contact_repository.hpp"
#include "domain/repository/group_repository.hpp"
#include "domain/repository/message_repository.hpp"
#include "domain/repository/talk_repository.hpp"
#include "domain/service/talk_service.hpp"

namespace IM::app {

class TalkServiceImpl : public IM::domain::service::ITalkService {
   public:
    explicit TalkServiceImpl(IM::domain::repository::ITalkRepository::Ptr talk_repo,
                             IM::domain::repository::IContactRepository::Ptr contact_repo,
                             IM::domain::repository::IMessageRepository::Ptr message_repo,
                             IM::domain::repository::IGroupRepository::Ptr group_repo);

    // 获取用户的会话列表
    Result<std::vector<dto::TalkSessionItem>> getSessionListByUserId(
        const uint64_t user_id) override;

    // 设置会话置顶/取消置顶
    Result<void> setSessionTop(const uint64_t user_id, const uint64_t to_from_id,
                               const uint8_t talk_mode, const uint8_t action) override;
    // 设置会话免打扰/取消免打扰
    Result<void> setSessionDisturb(const uint64_t user_id, const uint64_t to_from_id,
                                   const uint8_t talk_mode, const uint8_t action) override;

    // 创建会话
    Result<dto::TalkSessionItem> createSession(const uint64_t user_id, const uint64_t to_from_id,
                                               const uint8_t talk_mode) override;
    // 删除会话
    Result<void> deleteSession(const uint64_t user_id, const uint64_t to_from_id,
                               const uint8_t talk_mode) override;

    // 清除会话未读消息数
    Result<void> clearSessionUnreadNum(const uint64_t user_id, const uint64_t to_from_id,
                                       const uint8_t talk_mode) override;

   private:
    IM::domain::repository::ITalkRepository::Ptr m_talk_repo;
    IM::domain::repository::IContactRepository::Ptr m_contact_repo;
    IM::domain::repository::IMessageRepository::Ptr m_message_repo;
    IM::domain::repository::IGroupRepository::Ptr m_group_repo;
};

}  // namespace IM::app

#endif  // __IM_APP_TALK_SERVICE_IMPL_HPP__