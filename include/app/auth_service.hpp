#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "dao/user_dao.hpp"
#include "dao/user_login_log_dao.hpp"

namespace CIM::app {

struct AuthResult {
    bool ok = false;      // 是否成功
    std::string err;      // 错误描述
    CIM::dao::User user;  // 成功时的用户信息
};

class AuthService {
   public:
    // 注册新用户
    static AuthResult Register(const std::string& mobile, const std::string& password,
                               const std::string& email = std::string(),
                               const std::string& nickname = std::string());

    // 鉴权用户
    static AuthResult Authenticate(const std::string& mobile, const std::string& password,
                                   const std::string& platform);

    // 找回密码
    static AuthResult Forget(const std::string& mobile, const std::string& new_password);

    // 登录日志
    static bool LogLogin(const AuthResult& result, const std::string& platform, std::string* err = nullptr);
};

}  // namespace CIM::app
