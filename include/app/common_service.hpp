#pragma once

#include <string>

#include "dao/sms_code_dao.hpp"
#include "http/http_session.hpp"

namespace CIM::app {

struct SmsCodeResult {
    bool ok = false;             // 是否成功
    std::string err;             // 错误信息
    CIM::dao::SmsCode sms_code;  // 短信验证码信息
};

class CommonService {
   public:
    // 发送短信验证码
    static SmsCodeResult SendSmsCode(const std::string& mobile, const std::string& channel,
                                     CIM::http::HttpSession::ptr session);

    // 验证短信验证码
    static SmsCodeResult VerifySmsCode(const std::string& mobile, const std::string& code,
                                       const std::string& channel);

    // 初始化验证码失效认证定时器
    static void InitCleanupTimer();

    // 初始化无效验证码删除定时器
    static void InitInvalidCodeCleanupTimer();

   private:
    // 实际发送短信
    static bool SendRealSms(const std::string& mobile, const std::string& sms_code,
                            const std::string& channel, std::string* err);

    // 阿里云短信发送
    static bool SendSmsViaAliyun(const std::string& mobile, const std::string& sms_code,
                                 const std::string& channel, std::string* err);

    // 腾讯云短信发送
    static bool SendSmsViaTencent(const std::string& mobile, const std::string& sms_code,
                                  const std::string& channel, std::string* err);
};

}  // namespace CIM::app