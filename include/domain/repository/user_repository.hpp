#ifndef __IM_DOMAIN_REPOSITORY_USER_REPOSITORY_HPP__
#define __IM_DOMAIN_REPOSITORY_USER_REPOSITORY_HPP__

#include <memory>
#include <string>

#include "dto/user_dto.hpp"
#include "db/mysql.hpp"
#include "model/user.hpp"
#include "model/user_auth.hpp"
#include "model/user_login_log.hpp"
#include "model/user_settings.hpp"

namespace IM::domain::repository {

class IUserRepository {
   public:
    using Ptr = std::shared_ptr<IUserRepository>;
    virtual ~IUserRepository() = default;

    // 创建新用户
    virtual bool CreateUser(const std::shared_ptr<IM::MySQL>& db, const model::User& u,
                            uint64_t& out_id, std::string* err = nullptr) = 0;

    // 根据手机号获取用户信息
    virtual bool GetUserByMobile(const std::string& mobile, model::User& out,
                                 std::string* err = nullptr) = 0;

    // 根据邮箱获取用户信息
    virtual bool GetUserByEmail(const std::string& email, model::User& out,
                                std::string* err = nullptr) = 0;

    // 根据用户ID获取用户信息
    virtual bool GetUserById(const uint64_t id, model::User& out, std::string* err = nullptr) = 0;

    // 更新用户信息（昵称、头像、签名等）
    virtual bool UpdateUserInfo(const uint64_t id, const std::string& nickname,
                                const std::string& avatar, const std::string& avatar_media_id,
                                const std::string& motto, const uint8_t gender,
                                const std::string& birthday, std::string* err = nullptr) = 0;
   virtual bool UpdateUserInfoWithConn(const std::shared_ptr<IM::MySQL>& db, const uint64_t id,
                              const std::string& nickname, const std::string& avatar,
                              const std::string& avatar_media_id, const std::string& motto,
                              const uint8_t gender, const std::string& birthday,
                              std::string* err = nullptr) = 0;

    // 更新用户手机号
    virtual bool UpdateMobile(const uint64_t id, const std::string& new_mobile,
                              std::string* err = nullptr) = 0;

    // 更新用户邮箱
    virtual bool UpdateEmail(const uint64_t id, const std::string& new_email,
                             std::string* err = nullptr) = 0;

    // 用户上线
    virtual bool UpdateOnlineStatus(const uint64_t id, std::string* err = nullptr) = 0;

    // 用户下线
    virtual bool UpdateOfflineStatus(const uint64_t id, std::string* err = nullptr) = 0;

    // 获取用户在线状态
    virtual bool GetOnlineStatus(const uint64_t id, std::string& out_status,
                                 std::string* err = nullptr) = 0;

    // 获取用户配置信息
    virtual bool GetUserInfoSimple(const uint64_t uid, dto::UserInfo& out,
                                   std::string* err = nullptr) = 0;

    // 记录登录日志
    virtual bool CreateUserLoginLog(const model::UserLoginLog& log, std::string* err = nullptr) = 0;

    // 创建用户认证记录
    virtual bool CreateUserAuth(const std::shared_ptr<IM::MySQL>& db, const model::UserAuth& ua,
                                std::string* err = nullptr) = 0;

    // 根据用户 ID 获取用户认证记录
    virtual bool GetUserAuthById(const uint64_t user_id, model::UserAuth& out,
                                 std::string* err = nullptr) = 0;
    // 更新用户密码哈希
    virtual bool UpdatePasswordHash(const uint64_t user_id, const std::string& new_password_hash,
                                    std::string* err = nullptr) = 0;

    // 创建或更新用户设置
    virtual bool UpsertUserSettings(const model::UserSettings& settings,
                                    std::string* err = nullptr) = 0;

    // 获取用户配置详情
    virtual bool GetUserSettings(uint64_t user_id, model::UserSettings& out,
                                 std::string* err = nullptr) = 0;
};

}  // namespace IM::domain::repository

#endif  // __IM_DOMAIN_REPOSITORY_USER_REPOSITORY_HPP__