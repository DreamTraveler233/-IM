#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace CIM::dao {

struct SmsCode {
    uint64_t id = 0;             // 验证码ID
    std::string mobile;          // 手机号
    std::string sms_code;        // 验证码内容
    std::string channel;         // 发送渠道：aliyun, tencent等
    int32_t status = 0;          // 状态：0待验证 1成功 2失效
    std::time_t expired_at = 0;  // 过期时间
    std::time_t used_at = 0;     // 使用时间
    std::string send_ip;         // 发送请求IP
    std::time_t created_at = 0;  // 创建时间
};

class SmsCodeDAO {
   public:
    // 创建新的短信验证码
    static bool Create(const SmsCode& code, uint64_t& out_id, std::string* err = nullptr);

    // 根据手机号和验证码验证
    static bool Verify(const std::string& mobile, const std::string& code,
                       const std::string& channel, std::string* err = nullptr);

    // 标记验证码为已使用
    static bool MarkAsUsed(uint64_t id, std::string* err = nullptr);

    // 将过期验证码标记为失效
    static bool MarkExpiredAsInvalid(std::string* err = nullptr);

    // 删除失效验证码
    static bool DeleteInvalidCodes(std::string* err = nullptr);
};

}  // namespace CIM::dao