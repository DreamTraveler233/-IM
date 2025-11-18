#ifndef __CIM_APP_AUTH_SERVICE_HPP__
#define __CIM_APP_AUTH_SERVICE_HPP__

#include <cstdint>
#include <string>

#include "http/http_session.hpp"
#include "result.hpp"

namespace CIM::app {

class AuthService {
   public:
    // 鉴权用户
    static UserResult Authenticate(const std::string& mobile, const std::string& password,
                                   const std::string& platform);

    // 记录登录日志
    static VoidResult LogLogin(const UserResult& result, const std::string& platform,
                               CIM::http::HttpSession::ptr session);

    // 用户上线
    static VoidResult GoOnline(const uint64_t id);

    // 注册新用户
    static UserResult Register(const std::string& nickname, const std::string& mobile,
                               const std::string& password, const std::string& platform);

    // 找回密码
    static UserResult Forget(const std::string& mobile, const std::string& new_password);
};

}  // namespace CIM::app

#endif  // __CIM_APP_AUTH_SERVICE_HPP__