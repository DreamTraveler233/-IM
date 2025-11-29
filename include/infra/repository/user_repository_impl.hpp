#ifndef __IM_INFRA_REPOSITORY_USER_REPOSITORY_IMPL_HPP__
#define __IM_INFRA_REPOSITORY_USER_REPOSITORY_IMPL_HPP__

#include "db/mysql.hpp"
#include "domain/repository/user_repository.hpp"

namespace IM::infra::repository {

class UserRepositoryImpl : public IM::domain::repository::IUserRepository {
   public:
    explicit UserRepositoryImpl(std::shared_ptr<IM::MySQLManager> db_manager);
    bool CreateUser(const std::shared_ptr<IM::MySQL>& db, const model::User& u, uint64_t& out_id,
                    std::string* err = nullptr) override;
    bool GetUserByMobile(const std::string& mobile, model::User& out,
                         std::string* err = nullptr) override;
    bool GetUserByEmail(const std::string& email, model::User& out,
                        std::string* err = nullptr) override;
    bool GetUserById(const uint64_t id, model::User& out, std::string* err = nullptr) override;
    bool UpdateUserInfo(const uint64_t id, const std::string& nickname, const std::string& avatar,
                        const std::string& avatar_media_id, const std::string& motto,
                        const uint8_t gender, const std::string& birthday,
                        std::string* err = nullptr) override;
    bool UpdateUserInfoWithConn(const std::shared_ptr<IM::MySQL>& db, const uint64_t id,
                                const std::string& nickname, const std::string& avatar,
                                const std::string& avatar_media_id, const std::string& motto,
                                const uint8_t gender, const std::string& birthday,
                                std::string* err = nullptr) override;
    bool UpdateMobile(const uint64_t id, const std::string& new_mobile,
                      std::string* err = nullptr) override;
    bool UpdateEmail(const uint64_t id, const std::string& new_email,
                     std::string* err = nullptr) override;
    bool UpdateOnlineStatus(const uint64_t id, std::string* err = nullptr) override;
    bool UpdateOfflineStatus(const uint64_t id, std::string* err = nullptr) override;
    bool GetOnlineStatus(const uint64_t id, std::string& out_status,
                         std::string* err = nullptr) override;
    bool GetUserInfoSimple(const uint64_t uid, dto::UserInfo& out,
                           std::string* err = nullptr) override;
    bool CreateUserLoginLog(const model::UserLoginLog& log, std::string* err = nullptr) override;
    bool CreateUserAuth(const std::shared_ptr<IM::MySQL>& db, const model::UserAuth& ua,
                        std::string* err = nullptr) override;
    bool GetUserAuthById(const uint64_t user_id, model::UserAuth& out,
                         std::string* err = nullptr) override;
    bool UpdatePasswordHash(const uint64_t user_id, const std::string& new_password_hash,
                            std::string* err = nullptr) override;
    bool UpsertUserSettings(const model::UserSettings& settings,
                            std::string* err = nullptr) override;
    bool GetUserSettings(uint64_t user_id, model::UserSettings& out,
                         std::string* err = nullptr) override;

   private:
    std::shared_ptr<IM::MySQLManager> m_db_manager;
};

}  // namespace IM::infra::repository

#endif  // __IM_INFRA_REPOSITORY_USER_REPOSITORY_IMPL_HPP__