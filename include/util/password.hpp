#pragma once

#include <cstdint>
#include <string>

namespace CIM::util {

// 使用PBKDF2-HMAC-SHA256和随机盐值的密码哈希工具
class Password {
   public:
    /**
     * @brief 从明文密码派生密码哈希字符串
     * @param password 明文密码字符串
     * @param iterations 迭代次数，默认为120000
     * @return 返回格式为"pbkdf2_sha256$<iterations>$<salt_hex>$<hash_hex>"的哈希字符串
     */
    static std::string Hash(const std::string& password, uint32_t iterations = 120000);

    /**
     * @brief 验证明文密码与存储的哈希字符串是否匹配
     * @param password 待验证的明文密码
     * @param stored_hash 存储的哈希字符串，格式为"pbkdf2_sha256$<iterations>$<salt_hex>$<hash_hex>"
     * @return 匹配成功返回true，否则返回false
     */
    static bool Verify(const std::string& password, const std::string& stored_hash);
};

}  // namespace CIM::util