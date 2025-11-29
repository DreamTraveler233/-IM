#include "app/common_service_impl.hpp"

#include "base/macro.hpp"
#include "config/config.hpp"
#include "email/email.hpp"
#include "email/smtp.hpp"

namespace IM::app {

auto g_logger = IM_LOG_NAME("root");

static auto g_sms_enabled = IM::Config::Lookup<bool>("sms.enabled", false, "enable sms sending");
static auto g_sms_provider =
    IM::Config::Lookup<std::string>("sms.provider", "mock", "sms provider: aliyun/tencent/mock");
static auto g_sms_code_ttl_secs =
    IM::Config::Lookup<uint32_t>("sms.code_ttl_secs", 60, "sms code time to live in seconds");
static auto g_sms_code_cleanup_interval = IM::Config::Lookup<uint32_t>(
    "sms.code_cleanup_interval", 60, "sms code cleanup interval in seconds");

static auto g_email_enabled =
    IM::Config::Lookup<bool>("email.enabled", false, "enable email sending");
static auto g_smtp_host = IM::Config::Lookup<std::string>("smtp.host", "", "smtp host");
static auto g_smtp_port = IM::Config::Lookup<uint32_t>("smtp.port", 25, "smtp port");
static auto g_smtp_ssl = IM::Config::Lookup<bool>("smtp.ssl", false, "smtp ssl");
static auto g_smtp_debug = IM::Config::Lookup<bool>("smtp.debug", false, "smtp debug mode");
static auto g_smtp_auth_user =
    IM::Config::Lookup<std::string>("smtp.auth.user", "", "smtp auth user");
static auto g_smtp_auth_pass =
    IM::Config::Lookup<std::string>("smtp.auth.pass", "", "smtp auth pass");
static auto g_smtp_from_name =
    IM::Config::Lookup<std::string>("smtp.from.name", "", "smtp from display name");
static auto g_smtp_from_address =
    IM::Config::Lookup<std::string>("smtp.from.address", "", "smtp from address");
static auto g_email_code_ttl_secs =
    IM::Config::Lookup<uint32_t>("email.code_ttl_secs", 300, "email code time to live in seconds");
static auto g_email_code_cleanup_interval = IM::Config::Lookup<uint32_t>(
    "email.code_cleanup_interval", 3600, "email code cleanup interval in seconds");

// 验证码失效认证定时器
static IM::Timer::ptr g_cleanup_timer;
// 无效验证码删除定时器
static IM::Timer::ptr g_invalid_code_cleanup_timer;

CommonServiceImpl::CommonServiceImpl(IM::domain::repository::ICommonRepository::Ptr common_repo)
    : m_common_repo(std::move(common_repo)) {}

Result<model::SmsVerifyCode> CommonServiceImpl::SendSmsCode(const std::string& mobile,
                                                            const std::string& channel,
                                                            IM::http::HttpSession::ptr session) {
    Result<model::SmsVerifyCode> result;
    std::string err;

    /* 生成6位数字验证码 */
    std::string sms_code = IM::random_string(6, "0123456789");
    if (sms_code.size() != 6) {
        result.code = 500;
        result.err = "验证码生成失败";
        return result;
    }

    /* 根据配置决定是否发送真实短信 */
    if (g_sms_enabled->getValue()) {
        if (!SendRealSms(mobile, sms_code, channel, &err)) {
            IM_LOG_ERROR(g_logger) << "发送短信失败: " << err;
            result.code = 500;
            result.err = "短信发送失败";
            return result;
        }
    } else {
        // 模拟模式：仅记录日志
        IM_LOG_INFO(g_logger) << "模拟发送短信验证码到 " << mobile << ": " << sms_code;
    }

    /*保存验证码*/
    result.data.mobile = mobile;
    result.data.channel = channel;
    result.data.code = sms_code;
    result.data.sent_ip = session->getRemoteAddressString();
    result.data.expire_at = TimeUtil::NowToS() + 300;  // 5分钟后过期
    if (!m_common_repo->CreateSmsCode(result.data, &err)) {
        IM_LOG_ERROR(g_logger) << "保存短信验证码失败: " << err;
        result.code = 500;
        result.err = "保存验证码失败";
        return result;
    }

    result.ok = true;
    return result;
}

Result<void> CommonServiceImpl::VerifySmsCode(const std::string& mobile, const std::string& code,
                                              const std::string& channel) {
    // 使用 DAO 层进行原子校验（同时校验未过期与未使用，并标记为已使用）
    Result<void> result;
    std::string err;

    if (!m_common_repo->VerifySmsCode(mobile, code, channel, &err)) {
        if (!err.empty()) {
            IM_LOG_WARN(g_logger) << "验证码校验失败: " << err;
            result.code = 400;
            result.err = "验证码不正确";
            return result;
        }
    }
    result.ok = true;
    return result;
}

Result<model::EmailVerifyCode> CommonServiceImpl::SendEmailCode(
    const std::string& email, const std::string& channel, IM::http::HttpSession::ptr session) {
    Result<model::EmailVerifyCode> result;
    std::string err;

    /* 生成6位数字验证码 */
    std::string code = IM::random_string(6, "0123456789");
    if (code.size() != 6) {
        result.code = 500;
        result.err = "验证码生成失败";
        return result;
    }

    /* 根据配置决定是否发送真实邮件 */
    if (g_email_enabled->getValue()) {
        std::string title = "【心语IM】验证码";
        std::string body =
            "尊敬的用户：\r\n\r\n您好！\r\n\r\n您正在进行邮箱验证操作，本次验证码为：" + code +
            "，请在5分钟内完成验证。\r\n\r\n如非本人操作，请忽略此邮件。\r\n\r\nIM即时通讯团队";
        if (!SendRealEmail(email, title, body, &err)) {
            IM_LOG_ERROR(g_logger) << "发送邮件失败: " << err;
            result.code = 500;
            result.err = "邮件发送失败";
            return result;
        }
    } else {
        // 模拟模式：仅记录日志
        IM_LOG_INFO(g_logger) << "模拟发送邮件验证码到 " << email << ": " << code;
    }

    /*保存验证码*/
    IM::model::EmailVerifyCode email_code;
    email_code.email = email;
    email_code.channel = channel;
    email_code.code = code;
    email_code.sent_ip = session->getRemoteAddressString();
    email_code.expire_at = TimeUtil::NowToS() + g_email_code_ttl_secs->getValue();

    if (!m_common_repo->CreateEmailCode(email_code, &err)) {
        IM_LOG_ERROR(g_logger) << "保存邮件验证码失败: " << err;
        result.code = 500;
        result.err = "保存验证码失败";
        return result;
    }

    result.data.code = code;  // 用于测试/模拟，或在严格安全策略下移除
    result.ok = true;
    return result;
}

Result<void> CommonServiceImpl::VerifyEmailCode(const std::string& email, const std::string& code,
                                                const std::string& channel) {
    Result<void> result;
    std::string err;

    if (!m_common_repo->VerifyEmailCode(email, code, channel, &err)) {
        if (!err.empty()) {
            IM_LOG_WARN(g_logger) << "邮箱验证码校验失败: " << err;
            result.code = 400;
            result.err = "验证码不正确";
            return result;
        }
    }
    result.ok = true;
    return result;
}

bool CommonServiceImpl::SendRealEmail(const std::string& email_addr, const std::string& title,
                                      const std::string& body, std::string* err) {
    auto smtp_host = g_smtp_host->getValue();
    auto smtp_port = g_smtp_port->getValue();
    auto smtp_ssl = g_smtp_ssl->getValue();
    auto smtp_user = g_smtp_auth_user->getValue();
    auto smtp_pass = g_smtp_auth_pass->getValue();
    auto smtp_from_addr = g_smtp_from_address->getValue();
    auto smtp_from_name = g_smtp_from_name->getValue();
    // 如果新键未设置，尝试从旧的 smtp_from 中解析出姓名和地址
    if (smtp_from_addr.empty()) {
        // 回退到认证用户
        smtp_from_addr = smtp_user;
    }
    if (smtp_from_name.empty()) {
        // 如果未提供则留空
    }
    std::string display_from;
    if (!smtp_from_name.empty()) {
        display_from = smtp_from_name + " <" + smtp_from_addr + ">";
    } else {
        display_from = smtp_from_addr;
    }
    auto smtp_debug = g_smtp_debug->getValue();

    if (smtp_host.empty()) {
        if (err) *err = "SMTP host is not configured";
        IM_LOG_ERROR(g_logger) << "SMTP host is not configured";
        return false;
    }

    // 不回退到旧配置。请在新配置中显式设置认证用户/地址。

    IM::EMail::ptr mail = IM::EMail::Create(display_from, smtp_pass, title, body, {email_addr});
    // 如果提供则显式设置认证用户（供 SmtpClient 用于 AUTH）；未提供则回退为空
    if (!smtp_user.empty()) {
        mail->setAuthUser(smtp_user);
    }
    if (!mail) {
        if (err) *err = "create email object failed";
        return false;
    }

    auto smtp = IM::SmtpClient::Create(smtp_host, smtp_port, smtp_ssl);
    if (!smtp) {
        if (err) *err = "create smtp client failed";
        return false;
    }

    auto r = smtp->send(mail, 10000, smtp_debug);
    if (!r || r->result != 0) {
        std::string error_msg = r ? r->msg : "unknown error";
        if (err) *err = error_msg;
        IM_LOG_ERROR(g_logger) << "smtp send fail: " << error_msg;
        if (smtp_debug) {
            IM_LOG_ERROR(g_logger) << "smtp debug info: " << smtp->getDebugInfo();
        }
        return false;
    }
    return true;
}

// 实际发送短信（根据提供商调用相应API）
bool CommonServiceImpl::SendRealSms(const std::string& mobile, const std::string& sms_code,
                                    const std::string& channel, std::string* err) {
    auto provider = g_sms_provider->getValue();
    if (provider == "aliyun") {
        return SendSmsViaAliyun(mobile, sms_code, channel, err);
    } else if (provider == "tencent") {
        return SendSmsViaTencent(mobile, sms_code, channel, err);
    } else {
        // 默认mock模式
        IM_LOG_INFO(IM_LOG_ROOT()) << "模拟发送短信验证码到 " << mobile << ": " << sms_code;
        return true;
    }
}

// 示例：阿里云短信发送（需要安装阿里云SDK并配置AK/SK）
bool CommonServiceImpl::SendSmsViaAliyun(const std::string& mobile, const std::string& sms_code,
                                         const std::string& channel, std::string* err) {
    // TODO: 实现阿里云短信发送逻辑
    // 1. 获取配置：access_key_id, access_key_secret, sign_name, template_code
    // 2. 调用阿里云SMS API发送短信
    // 3. 返回发送结果

    // 临时实现：模拟成功
    IM_LOG_INFO(g_logger) << "阿里云短信发送到 " << mobile << ": " << sms_code;
    return true;
}

// 示例：腾讯云短信发送
bool CommonServiceImpl::SendSmsViaTencent(const std::string& mobile, const std::string& sms_code,
                                          const std::string& channel, std::string* err) {
    // TODO: 实现腾讯云短信发送逻辑
    IM_LOG_INFO(g_logger) << "腾讯云短信发送到 " << mobile << ": " << sms_code;
    return true;
}

// 初始化验证码清理定时器
void CommonServiceImpl::InitCleanupTimer() {
    // 防止重复初始化
    if (g_cleanup_timer) {
        return;
    }
    // 定期将过期验证码标记为失效（取短信与邮箱中较小的 TTL，以保证及时处理）
    uint32_t cleanup_timer_secs =
        std::min(g_sms_code_ttl_secs->getValue(), g_email_code_ttl_secs->getValue());
    g_cleanup_timer = IM::IOManager::GetThis()->addTimer(
        cleanup_timer_secs * 1000,
        [this]() {
            std::string err;
            if (!m_common_repo->MarkSmsCodeExpiredAsInvalid(&err)) {
                IM_LOG_ERROR(IM_LOG_ROOT()) << "处理过期短信验证码失败: " << err;
            }
            if (!m_common_repo->MarkEmailCodeExpiredAsInvalid(&err)) {
                IM_LOG_ERROR(IM_LOG_ROOT()) << "处理过期邮箱验证码失败: " << err;
            }
        },
        true);  // 周期性执行
}

void CommonServiceImpl::InitInvalidCodeCleanupTimer() {
    // 防止重复初始化
    if (g_invalid_code_cleanup_timer) {
        return;
    }
    // 删除失效验证码（取短信/邮件中较小的清理间隔）
    uint32_t invalid_cleanup_secs = std::min(g_sms_code_cleanup_interval->getValue(),
                                             g_email_code_cleanup_interval->getValue());
    g_invalid_code_cleanup_timer = IM::IOManager::GetThis()->addTimer(
        invalid_cleanup_secs * 1000,
        [this]() {
            std::string err;
            if (!m_common_repo->DeleteInvalidSmsCode(&err)) {
                IM_LOG_ERROR(IM_LOG_ROOT()) << "处理失效短信验证码失败: " << err;
            }
            if (!m_common_repo->DeleteInvalidEmailCode(&err)) {
                IM_LOG_ERROR(IM_LOG_ROOT()) << "处理失效邮箱验证码失败: " << err;
            }
        },
        true);  // 周期性执行
}
}  // namespace IM::app