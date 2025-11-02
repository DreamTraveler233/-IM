#include "app/common_service.hpp"

#include "config/config.hpp"
#include "dao/sms_code_dao.hpp"
#include "io/iomanager.hpp"
#include "macro.hpp"

namespace CIM::app {

static auto g_sms_enabled = CIM::Config::Lookup<bool>("sms.enabled", false, "enable sms sending");
static auto g_sms_provider =
    CIM::Config::Lookup<std::string>("sms.provider", "mock", "sms provider: aliyun/tencent/mock");
static auto g_sms_code_ttl_secs =
    CIM::Config::Lookup<uint32_t>("sms.code_ttl_secs", 60, "sms code time to live in seconds");
static auto g_sms_code_cleanup_interval = CIM::Config::Lookup<uint32_t>(
    "sms.code_cleanup_interval", 60, "sms code cleanup interval in seconds");

// 验证码失效认证定时器
static CIM::Timer::ptr g_cleanup_timer;
// 无效验证码删除定时器
static CIM::Timer::ptr g_invalid_code_cleanup_timer;

SmsCodeResult CommonService::SendSmsCode(const std::string& mobile, const std::string& channel,
                                         CIM::http::HttpSession::ptr session) {
    SmsCodeResult result;

    /* 生成6位数字验证码 */
    std::string sms_code = CIM::random_string(6, "0123456789");
    if (sms_code.size() != 6) {
        result.err = "验证码生成失败";
        return result;
    }

    /* 根据配置决定是否发送真实短信 */
    if (g_sms_enabled->getValue()) {
        std::string send_err;
        if (!SendRealSms(mobile, sms_code, channel, &send_err)) {
            result.err = send_err.empty() ? "短信发送失败" : send_err;
            return result;
        }
    } else {
        // 模拟模式：仅记录日志
        CIM_LOG_INFO(CIM_LOG_ROOT()) << "模拟发送短信验证码到 " << mobile << ": " << sms_code;
    }

    /*保存验证码*/
    CIM::dao::SmsCode code;
    code.mobile = mobile;
    code.sms_code = sms_code;
    code.channel = channel;
    code.expired_at = TimeUtil::NowToS() + 300;  // 5分钟后过期
    code.send_ip = session->getRemoteAddressString();
    code.created_at = TimeUtil::NowToS();
    if (!CIM::dao::SmsCodeDAO::Create(code, code.id, &result.err)) {
        return result;
    }

    result.sms_code = std::move(code);
    result.ok = true;
    return result;
}

SmsCodeResult CommonService::VerifySmsCode(const std::string& mobile, const std::string& code,
                                           const std::string& channel) {
    // 使用 DAO 层进行原子校验（同时校验未过期与未使用，并标记为已使用）
    SmsCodeResult result;
    std::string err;
    if (!CIM::dao::SmsCodeDAO::Verify(mobile, code, channel, &err)) {
        // 统一返回提示，优先使用具体错误信息
        result.err = "验证码不正确";
        CIM_LOG_WARN(CIM_LOG_ROOT()) << "验证码校验失败: " << err;
        return result;
    }

    result.ok = true;
    return result;
}

// 实际发送短信（根据提供商调用相应API）
bool CommonService::SendRealSms(const std::string& mobile, const std::string& sms_code,
                                const std::string& channel, std::string* err) {
    auto provider = g_sms_provider->getValue();
    if (provider == "aliyun") {
        return SendSmsViaAliyun(mobile, sms_code, channel, err);
    } else if (provider == "tencent") {
        return SendSmsViaTencent(mobile, sms_code, channel, err);
    } else {
        // 默认mock模式
        CIM_LOG_INFO(CIM_LOG_ROOT()) << "模拟发送短信验证码到 " << mobile << ": " << sms_code;
        return true;
    }
}

// 示例：阿里云短信发送（需要安装阿里云SDK并配置AK/SK）
bool CommonService::SendSmsViaAliyun(const std::string& mobile, const std::string& sms_code,
                                     const std::string& channel, std::string* err) {
    // TODO: 实现阿里云短信发送逻辑
    // 1. 获取配置：access_key_id, access_key_secret, sign_name, template_code
    // 2. 调用阿里云SMS API发送短信
    // 3. 返回发送结果

    // 临时实现：模拟成功
    CIM_LOG_INFO(CIM_LOG_ROOT()) << "阿里云短信发送到 " << mobile << ": " << sms_code;
    return true;
}

// 示例：腾讯云短信发送
bool CommonService::SendSmsViaTencent(const std::string& mobile, const std::string& sms_code,
                                      const std::string& channel, std::string* err) {
    // TODO: 实现腾讯云短信发送逻辑
    CIM_LOG_INFO(CIM_LOG_ROOT()) << "腾讯云短信发送到 " << mobile << ": " << sms_code;
    return true;
}

// 初始化验证码清理定时器
void CommonService::InitCleanupTimer() {
    // 防止重复初始化
    if (g_cleanup_timer) {
        return;
    }
    // 每1分钟将过期验证码标记为失效
    g_cleanup_timer = CIM::IOManager::GetThis()->addTimer(
        g_sms_code_ttl_secs->getValue() * 1000,
        []() {
            std::string err;
            if (!CIM::dao::SmsCodeDAO::MarkExpiredAsInvalid(&err)) {
                CIM_LOG_ERROR(CIM_LOG_ROOT()) << "处理过期验证码失败: " << err;
            } else {
                CIM_LOG_INFO(CIM_LOG_ROOT()) << "成功处理过期验证码";
            }
        },
        true);  // 周期性执行
}

void CommonService::InitInvalidCodeCleanupTimer() {
    // 防止重复初始化
    if (g_invalid_code_cleanup_timer) {
        return;
    }
    // 每1小时删除失效验证码
    g_invalid_code_cleanup_timer = CIM::IOManager::GetThis()->addTimer(
        g_sms_code_cleanup_interval->getValue() * 1000,
        []() {
            std::string err;
            if (!CIM::dao::SmsCodeDAO::DeleteInvalidCodes(&err)) {
                CIM_LOG_ERROR(CIM_LOG_ROOT()) << "处理失效验证码失败: " << err;
            } else {
                CIM_LOG_INFO(CIM_LOG_ROOT()) << "成功处理失效验证码";
            }
        },
        true);  // 周期性执行
}

}  // namespace CIM::app
