#include "util/password.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <iomanip>
#include <sstream>
#include <vector>

namespace CIM {
namespace util {

namespace {

/**
 * @brief 将字节数组转换为十六进制字符串
 * @param data 待转换的字节数组
 * @param len 字节数组长度
 * @return 转换后的十六进制字符串
 */
static std::string to_hex(const unsigned char* data, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        oss << std::setw(2) << static_cast<unsigned int>(data[i]);
    }
    return oss.str();
}

/**
 * @brief 将十六进制字符串转换为字节数组
 * @param hex 十六进制字符串
 * @param out 输出的字节数组
 * @return 转换成功返回true，否则返回false
 */
static bool from_hex(const std::string& hex, std::vector<unsigned char>& out) {
    // 检查十六进制字符串长度是否为偶数
    if (hex.size() % 2 != 0) return false;
    out.resize(hex.size() / 2);
    for (size_t i = 0; i < out.size(); ++i) {
        unsigned int byte;
        // 从十六进制字符串中提取字节数据
        if (sscanf(hex.c_str() + i * 2, "%02x", &byte) != 1) {
            return false;
        }
        out[i] = static_cast<unsigned char>(byte);
    }
    return true;
}

}  // namespace

std::string Password::Hash(const std::string& password, uint32_t iterations) {
    const size_t salt_len = 16;  // 128-bit salt
    const size_t dk_len = 32;    // 256-bit derived key

    // 生成随机盐值
    unsigned char salt[salt_len];
    if (RAND_bytes(salt, static_cast<int>(salt_len)) != 1) {
        return std::string();
    }

    // 使用PBKDF2算法生成密钥
    std::vector<unsigned char> dk(dk_len);
    int ok = PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()), salt,
                               static_cast<int>(salt_len), static_cast<int>(iterations),
                               EVP_sha256(), static_cast<int>(dk_len), dk.data());
    if (ok != 1) {
        return std::string();
    }

    // 构造存储格式的哈希字符串
    std::ostringstream oss;
    oss << "pbkdf2_sha256$" << iterations << "$" << to_hex(salt, salt_len) << "$"
        << to_hex(dk.data(), dk_len);
    return oss.str();
}

bool Password::Verify(const std::string& password, const std::string& stored_hash) {
    // expected format: pbkdf2_sha256$<iterations>$<salt_hex>$<hash_hex>
    const std::string prefix = "pbkdf2_sha256$";
    // 检查哈希字符串是否以正确的前缀开头
    if (stored_hash.rfind(prefix, 0) != 0) return false;

    // 解析迭代次数
    size_t p1 = stored_hash.find('$', prefix.size());
    if (p1 == std::string::npos) return false;
    std::string iter_str = stored_hash.substr(prefix.size(), p1 - prefix.size());

    // 解析盐值
    size_t p2 = stored_hash.find('$', p1 + 1);
    if (p2 == std::string::npos) return false;
    std::string salt_hex = stored_hash.substr(p1 + 1, p2 - (p1 + 1));

    // 解析哈希值
    std::string dk_hex = stored_hash.substr(p2 + 1);

    // 转换迭代次数
    uint32_t iterations = 0;
    try {
        iterations = static_cast<uint32_t>(std::stoul(iter_str));
    } catch (...) {
        return false;
    }

    // 转换盐值
    std::vector<unsigned char> salt;
    if (!from_hex(salt_hex, salt)) return false;

    // 转换参考哈希值
    std::vector<unsigned char> dk_ref;
    if (!from_hex(dk_hex, dk_ref)) return false;

    // 使用提供的密码和解析出的盐值重新计算哈希
    std::vector<unsigned char> dk(dk_ref.size());
    int ok = PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()), salt.data(),
                               static_cast<int>(salt.size()), static_cast<int>(iterations),
                               EVP_sha256(), static_cast<int>(dk.size()), dk.data());
    if (ok != 1) return false;

    // 检查长度是否匹配
    if (dk.size() != dk_ref.size()) return false;
    // 恒定时间比较，防止时序攻击
    unsigned char diff = 0;
    for (size_t i = 0; i < dk.size(); ++i) diff |= (dk[i] ^ dk_ref[i]);
    return diff == 0;
}

}  // namespace util
}  // namespace CIM