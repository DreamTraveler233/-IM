#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace CIM::dao {

struct UserToken {
    uint64_t id = 0;               // 令牌记录ID
    uint64_t user_id = 0;          // 用户ID
    std::string access_token;      // 访问令牌
    std::string platform = "web";  // 平台：web, mobile等
    std::string client_ip;         // 客户端IP地址
    std::time_t expires_at = 0;    // 过期时间
    std::time_t revoked_at = 0;    // 撤销时间（软删除）
    std::time_t created_at = 0;    // 创建时间
};

class UserTokenDAO {
   public:
    // 创建新的用户令牌
    static bool Create(const UserToken& token, uint64_t& out_id, std::string* err = nullptr);

    // 根据令牌获取令牌信息
    static bool GetByToken(const std::string& access_token, UserToken& out);

    // 根据用户ID获取所有有效令牌
    static bool GetByUserId(uint64_t user_id, std::vector<UserToken>& out);

    // 撤销令牌（软删除）
    static bool RevokeToken(const std::string& access_token, std::string* err = nullptr);

    // 撤销用户的所有令牌
    static bool RevokeAllUserTokens(uint64_t user_id, std::string* err = nullptr);

    // 清理过期令牌
    static bool CleanupExpiredTokens(std::string* err = nullptr);
};

}  // namespace CIM::dao