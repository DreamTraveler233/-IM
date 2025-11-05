#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "dao/user_dao.hpp"
#include "dao/user_login_log_dao.hpp"

namespace CIM::app {

struct UserResult {
    bool ok = false;      // 是否成功
    std::string err;      // 错误描述
    CIM::dao::User user;  // 成功时的用户信息
};

class AuthService {
   public:
    // 注册新用户
    static UserResult Register(const std::string& nickname, const std::string& mobile,
                               const std::string& password, const std::string& platform);

    // 鉴权用户
    static UserResult Authenticate(const std::string& mobile, const std::string& password,
                                   const std::string& platform);

    // 找回密码
    static UserResult Forget(const std::string& mobile, const std::string& new_password);

    // 登录日志
    static bool LogLogin(const UserResult& result, const std::string& platform,
                         std::string* err = nullptr);
};

}  // namespace CIM::app
