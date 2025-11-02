#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace CIM::dao {

struct UserLoginLog {
    uint64_t id = 0;           // 日志ID
    uint64_t user_id = 0;      // 用户ID
    std::string platform;      // 登录平台
    std::time_t login_at = 0;  // 登录时间
    int32_t status = 1;        // 登录状态：1成功 2失败
};

class UserLoginLogDAO {
   public:
    // 记录登录日志
    static bool Create(const UserLoginLog& log, uint64_t& out_id, std::string* err = nullptr);
};

}  // namespace CIM::dao