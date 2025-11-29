#ifndef __IM_APP_USER_SERVICE_IMPL_HPP__
#define __IM_APP_USER_SERVICE_IMPL_HPP__

#include "domain/repository/user_repository.hpp"
#include "domain/repository/talk_repository.hpp"
#include "domain/service/common_service.hpp"
#include "domain/service/media_service.hpp"
#include "domain/service/user_service.hpp"

namespace IM::app {

class UserServiceImpl : public IM::domain::service::IUserService {
   public:
    explicit UserServiceImpl(IM::domain::repository::IUserRepository::Ptr user_repo,
                             IM::domain::service::IMediaService::Ptr media_service,
                             IM::domain::service::ICommonService::Ptr common_service,
                             IM::domain::repository::ITalkRepository::Ptr talk_repo);

    Result<model::User> LoadUserInfo(const uint64_t uid) override;
    Result<void> UpdatePassword(const uint64_t uid, const std::string& old_password,
                                const std::string& new_password) override;
    Result<void> UpdateUserInfo(const uint64_t uid, const std::string& nickname,
                                const std::string& avatar, const std::string& motto,
                                const uint32_t gender, const std::string& birthday) override;
    Result<void> UpdateMobile(const uint64_t uid, const std::string& password,
                              const std::string& new_mobile, const std::string& sms_code) override;
    Result<void> UpdateEmail(const uint64_t uid, const std::string& password,
                             const std::string& new_email, const std::string& email_code) override;
    Result<model::User> GetUserByMobile(const std::string& mobile,
                                        const std::string& channel) override;
    Result<model::User> GetUserByEmail(const std::string& email,
                                       const std::string& channel) override;
    Result<void> Offline(const uint64_t id) override;
    Result<std::string> GetUserOnlineStatus(const uint64_t id) override;
    Result<void> SaveConfigInfo(const uint64_t user_id, const std::string& theme_mode,
                                const std::string& theme_bag_img, const std::string& theme_color,
                                const std::string& notify_cue_tone,
                                const std::string& keyboard_event_notify) override;
    Result<model::UserSettings> LoadConfigInfo(const uint64_t user_id) override;
    Result<dto::UserInfo> LoadUserInfoSimple(const uint64_t uid) override;
    Result<model::User> Authenticate(const std::string& mobile, const std::string& password,
                                     const std::string& platform) override;
    Result<void> LogLogin(const Result<model::User>& result, const std::string& platform,
                          IM::http::HttpSession::ptr session) override;
    Result<void> GoOnline(const uint64_t id) override;
    Result<model::User> Register(const std::string& nickname, const std::string& mobile,
                                 const std::string& password, const std::string& sms_code,
                                 const std::string& platform) override;
    Result<model::User> Forget(const std::string& mobile, const std::string& new_password,
                               const std::string& sms_code) override;

   private:
    IM::domain::repository::IUserRepository::Ptr m_user_repo;
    IM::domain::service::IMediaService::Ptr m_media_service;
    IM::domain::service::ICommonService::Ptr m_common_service;
    IM::domain::repository::ITalkRepository::Ptr m_talk_repo;
};

}  // namespace IM::app

#endif  // __IM_APP_USER_SERVICE_IMPL_HPP__