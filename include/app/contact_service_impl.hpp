#ifndef __IM_APP_CONTACT_SERVICE_IMPL_HPP__
#define __IM_APP_CONTACT_SERVICE_IMPL_HPP__

#include "domain/repository/contact_repository.hpp"
#include "domain/repository/talk_repository.hpp"
#include "domain/repository/user_repository.hpp"
#include "domain/service/contact_service.hpp"
#include "domain/service/message_service.hpp"
#include "domain/service/talk_service.hpp"
namespace IM::app {

class ContactServiceImpl : public IM::domain::service::IContactService {
   public:
    explicit ContactServiceImpl(IM::domain::repository::IContactRepository::Ptr contact_repo,
                                IM::domain::repository::IUserRepository::Ptr user_repo,
                                IM::domain::repository::ITalkRepository::Ptr talk_repo,
                                IM::domain::service::IMessageService::Ptr message_service,
                                IM::domain::service::ITalkService::Ptr talk_service);

    Result<dto::TalkSessionItem> AgreeApply(const uint64_t user_id, const uint64_t apply_id,
                                            const std::string& remark) override;
    Result<model::User> SearchByMobile(const std::string& mobile) override;
    Result<dto::ContactDetails> GetContactDetail(const uint64_t user_id,
                                                 const uint64_t target_id) override;
    Result<std::vector<dto::ContactItem>> ListFriends(const uint64_t user_id) override;
    Result<void> CreateContactApply(const uint64_t apply_user_id, const uint64_t target_user_id,
                                    const std::string& remark) override;
    Result<uint64_t> GetPendingContactApplyCount(const uint64_t user_id) override;
    Result<std::vector<dto::ContactApplyItem>> ListContactApplies(const uint64_t user_id) override;
    Result<void> RejectApply(const uint64_t handler_user_id, const uint64_t apply_user_id,
                             const std::string& remark) override;
    Result<void> EditContactRemark(const uint64_t user_id, const uint64_t contact_id,
                                   const std::string& remark) override;
    Result<void> DeleteContact(const uint64_t user_id, const uint64_t contact_id) override;
    Result<void> SaveContactGroup(
        const uint64_t user_id,
        const std::vector<std::tuple<uint64_t, uint64_t, std::string>>& groupItems) override;
    Result<std::vector<dto::ContactGroupItem>> GetContactGroupLists(
        const uint64_t user_id) override;
    Result<void> ChangeContactGroup(const uint64_t user_id, const uint64_t contact_id,
                                    const uint64_t group_id) override;

   private:
    IM::domain::repository::IContactRepository::Ptr m_contact_repo;
    IM::domain::repository::IUserRepository::Ptr m_user_repo;
    IM::domain::repository::ITalkRepository::Ptr m_talk_repo;
    IM::domain::service::IMessageService::Ptr m_message_service;
    IM::domain::service::ITalkService::Ptr m_talk_service;
};

}  // namespace IM::app

#endif  // __IM_APP_CONTACT_SERVICE_IMPL_HPP__