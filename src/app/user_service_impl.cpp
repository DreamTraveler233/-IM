#include "app/user_service_impl.hpp"

#include <jsoncpp/json/json.h>

#include "base/macro.hpp"
#include "db/mysql.hpp"
#include "infra/repository/user_repository_impl.hpp"
#include "model/media_file.hpp"
#include "util/password.hpp"
#include "util/security_util.hpp"
#include "api/ws_gateway_module.hpp"
#include "util/time_util.hpp"

namespace IM::app {

static auto g_logger = IM_LOG_NAME("system");
static constexpr const char* kDBName = "default";

UserServiceImpl::UserServiceImpl(IM::domain::repository::IUserRepository::Ptr user_repo,
                                                                 IM::domain::service::IMediaService::Ptr media_service,
                                                                 IM::domain::service::ICommonService::Ptr common_service,
                                                                 IM::domain::repository::ITalkRepository::Ptr talk_repo)
        : m_user_repo(std::move(user_repo)),
            m_media_service(std::move(media_service)),
            m_common_service(std::move(common_service)),
            m_talk_repo(std::move(talk_repo)) {}

Result<model::User> UserServiceImpl::LoadUserInfo(const uint64_t uid) {
    Result<model::User> result;
    std::string err;

    if (!m_user_repo->GetUserById(uid, result.data, &err)) {
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger)
                << "LoadUserInfo GetUserById failed, uid=" << uid << ", err=" << err;
            result.code = 500;
            result.err = "加载用户信息失败";
            return result;
        }
    }

    if (!result.data.avatar.empty() && result.data.avatar.length() == 32) {
        bool is_id = true;
        for (char c : result.data.avatar) {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                is_id = false;
                break;
            }
        }
        if (is_id) {
            IM::model::MediaFile media;
            auto gf_res = m_media_service->GetMediaFile(result.data.avatar);
            if (gf_res.ok) {
                result.data.avatar = gf_res.data.url;
            } else {
                IM_LOG_WARN(g_logger) << "LoadUserInfo resolve avatar id failed: " << result.data.avatar << ", err=" << gf_res.err;
            }
        }
    }

    result.ok = true;
    return result;
}

Result<void> UserServiceImpl::UpdateUserInfo(const uint64_t uid, const std::string& nickname,
                                             const std::string& avatar, const std::string& motto,
                                             const uint32_t gender, const std::string& birthday) {
    Result<void> result;
    std::string err;

    std::string real_avatar = avatar;
    std::string avatar_media_id;

    // 检查 avatar 是否为文件 ID (32位 hex)
    if (avatar.length() == 32) {
        bool is_id = true;
        for (char c : avatar) {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                is_id = false;
                break;
            }
        }
        if (is_id) {
            avatar_media_id = avatar;
            // 尝试解析为 URL
            auto gf_res = m_media_service->GetMediaFile(avatar);
            if (gf_res.ok) {
                real_avatar = gf_res.data.url;
            } else {
                IM_LOG_WARN(g_logger) << "UpdateUserInfo resolve avatar id failed: " << avatar << ", err=" << gf_res.err;
            }
        }
    }

    // 开启事务，保证用户更新与会话更新原子性
    auto trans = IM::MySQLMgr::GetInstance()->openTransaction(kDBName, false);
    if (!trans) {
        IM_LOG_ERROR(g_logger) << "UpdateUserInfo openTransaction failed, uid=" << uid;
        result.code = 500;
        result.err = "更新用户信息失败";
        return result;
    }
    auto db = trans->getMySQL();
    if (!db) {
        IM_LOG_ERROR(g_logger) << "UpdateUserInfo get transaction connection failed, uid=" << uid;
        result.code = 500;
        result.err = "更新用户信息失败";
        return result;
    }

    if (!m_user_repo->UpdateUserInfoWithConn(db, uid, nickname, real_avatar, avatar_media_id, motto, gender, birthday, &err)) {
        IM_LOG_ERROR(g_logger) << "UpdateUserInfo failed, uid=" << uid << ", err=" << err;
        trans->rollback();
        result.code = 500;
        result.err = "更新用户信息失败";
        return result;
    }
    // 更新会话快照中的 avatar 字段（对单聊：其它用户的会话中展示当前用户的头像）
    std::string err2;
    if (!m_talk_repo->updateSessionAvatarByTargetUserWithConn(db, uid, real_avatar, &err2)) {
        if (!err2.empty()) {
            IM_LOG_WARN(g_logger) << "Failed to update session avatar for user= " << uid << ", err=" << err2;
        } else {
            IM_LOG_WARN(g_logger) << "Failed to update session avatar for user= " << uid;
        }
        // 回滚事务以保证一致性
        trans->rollback();
        result.code = 500;
        result.err = "更新用户信息失败";
        return result;
    }

    // 查询受影响的用户（会话 owner ids），用于推送 WebSocket 更新
    std::vector<uint64_t> affected_user_ids;
    if (!m_talk_repo->listUsersByTargetUserWithConn(db, uid, affected_user_ids, &err2)) {
        IM_LOG_WARN(g_logger) << "listUsersByTargetUserWithConn err=" << err2;
        // 允许继续提交事务，尽管之后无法推送到特定用户
    }

    if (!trans->commit()) {
        const auto commit_err = db->getErrStr();
        trans->rollback();
        IM_LOG_ERROR(g_logger) << "UpdateUserInfo commit transaction failed, uid=" << uid
                               << ", err=" << commit_err;
        result.code = 500;
        result.err = "更新用户信息失败";
        return result;
    }

    // 提交成功后：向受影响用户推送会话更新事件
    Json::Value payload;
    payload["talk_mode"] = 1;
    payload["to_from_id"] = (Json::UInt64)uid;
    if (!real_avatar.empty())
        payload["avatar"] = real_avatar;
    else
        payload["avatar"] = Json::Value();
    payload["updated_at"] = IM::TimeUtil::NowToMS();
    for (auto user_id : affected_user_ids) {
        IM::api::WsGatewayModule::PushToUser(user_id, "im.session.update", payload);
        IM::api::WsGatewayModule::PushToUser(user_id, "im.session.reload", Json::Value());
    }

    result.ok = true;
    return result;
}

Result<void> UserServiceImpl::UpdateMobile(const uint64_t uid, const std::string& password,
                                           const std::string& new_mobile,
                                           const std::string& sms_code) {
    Result<void> result;
    std::string err;

    auto sms_result = m_common_service->VerifySmsCode(new_mobile, sms_code, "mobile_update");
    if (!sms_result.ok) {
        result.code = sms_result.code;
        result.err = sms_result.err;
        return result;
    }

    // 解密前端传入的登录密码
    std::string decrypted_password;
    auto dec_res = IM::util::DecryptPassword(password, decrypted_password);
    if (!dec_res.ok) {
        result.code = dec_res.code;
        result.err = dec_res.err;
        return result;
    }

    // 加载当前用户信息用于校验
    IM::model::User user;
    if (!m_user_repo->GetUserById(uid, user, &err)) {
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger)
                << "UpdateMobile GetUserById failed, uid=" << uid << ", err=" << err;
            result.code = 404;
            result.err = "加载用户信息失败";
            return result;
        }
    }

    // 加载用户密码并验证
    IM::model::UserAuth ua;
    if (!m_user_repo->GetUserAuthById(uid, ua, &err)) {
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger)
                << "UpdateMobile GetUserAuthById failed, uid=" << uid << ", err=" << err;
            result.code = 500;
            result.err = "加载用户认证信息失败";
            return result;
        }
    }

    if (!IM::util::Password::Verify(decrypted_password, ua.password_hash)) {
        result.code = 403;
        result.err = "密码错误";
        return result;
    }

    if (user.mobile == new_mobile) {
        result.code = 400;
        result.err = "新手机号不能与原手机号相同";
        return result;
    }

    // 检查新手机号是否已被其他账户使用
    IM::model::User other_user;
    if (m_user_repo->GetUserByMobile(new_mobile, other_user, &err)) {
        if (other_user.id != uid) {
            result.code = 400;
            result.err = "新手机号已被使用";
            return result;
        }
    }

    if (!m_user_repo->UpdateMobile(uid, new_mobile, &err)) {
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger)
                << "UpdateMobile UpdateMobile failed, uid=" << uid << ", err=" << err;
            result.code = 500;
            result.err = "更新手机号失败";
            return result;
        }
    }

    result.ok = true;
    return result;
}

Result<void> UserServiceImpl::UpdateEmail(const uint64_t uid, const std::string& password,
                                          const std::string& new_email,
                                          const std::string& email_code) {
    Result<void> result;
    std::string err;

    auto verify_result = m_common_service->VerifyEmailCode(new_email, email_code, "update_email");
    if (!verify_result.ok) {
        result.code = verify_result.code;
        result.err = verify_result.err;
        return result;
    }

    // 解密前端传入的登录密码
    std::string decrypted_password;
    auto dec_res = IM::util::DecryptPassword(password, decrypted_password);
    if (!dec_res.ok) {
        result.code = dec_res.code;
        result.err = dec_res.err;
        return result;
    }

    // 加载当前用户信息用于校验
    IM::model::User user;
    if (!m_user_repo->GetUserById(uid, user, &err)) {
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger)
                << "UpdateEmail GetUserById failed, uid=" << uid << ", err=" << err;
            result.code = 404;
            result.err = "加载用户信息失败";
            return result;
        }
    }

    // 加载用户密码并验证
    IM::model::UserAuth ua;
    if (!m_user_repo->GetUserAuthById(uid, ua, &err)) {
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger)
                << "UpdateEmail GetUserAuthById failed, uid=" << uid << ", err=" << err;
            result.code = 500;
            result.err = "加载用户认证信息失败";
            return result;
        }
    }

    if (!IM::util::Password::Verify(decrypted_password, ua.password_hash)) {
        result.code = 403;
        result.err = "密码错误";
        return result;
    }

    if (user.email == new_email) {
        result.code = 400;
        result.err = "新邮箱不能与原邮箱相同";
        return result;
    }

    // 检查新邮箱是否已被其他账户使用
    IM::model::User other_user;
    if (m_user_repo->GetUserByEmail(new_email, other_user, &err)) {
        if (other_user.id != uid) {
            result.code = 400;
            result.err = "新邮箱已被使用";
            return result;
        }
    }

    if (!m_user_repo->UpdateEmail(uid, new_email, &err)) {
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger)
                << "UpdateEmail UpdateEmail failed, uid=" << uid << ", err=" << err;
            result.code = 500;
            result.err = "更新邮箱失败";
            return result;
        }
    }

    result.ok = true;
    return result;
}

Result<void> UserServiceImpl::UpdatePassword(const uint64_t uid, const std::string& old_password,
                                             const std::string& new_password) {
    Result<void> result;
    std::string err;

    // 1、密码解密
    std::string decrypted_old, decrypted_new;
    auto dec_old_res = IM::util::DecryptPassword(old_password, decrypted_old);
    if (!dec_old_res.ok) {
        result.code = dec_old_res.code;
        result.err = dec_old_res.err;
        return result;
    }
    auto dec_new_res = IM::util::DecryptPassword(new_password, decrypted_new);
    if (!dec_new_res.ok) {
        result.code = dec_new_res.code;
        result.err = dec_new_res.err;
        return result;
    }

    // 2、验证旧密码
    IM::model::UserAuth ua;
    if (!m_user_repo->GetUserAuthById(uid, ua, &err)) {
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger)
                << "UpdatePassword GetUserAuthById failed, uid=" << uid << ", err=" << err;
            result.code = 500;
            result.err = "加载用户认证信息失败";
            return result;
        }
    }
    if (!IM::util::Password::Verify(decrypted_old, ua.password_hash)) {
        result.code = 403;
        result.err = "旧密码错误";
        return result;
    }

    // 3、生成新密码哈希
    auto new_password_hash = IM::util::Password::Hash(decrypted_new);
    if (new_password_hash.empty()) {
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger) << "UpdatePasswordHash Hash failed, uid=" << uid;
            result.code = 500;
            result.err = "新密码哈希生成失败";
            return result;
        }
    }

    // 4、更新新密码
    if (!m_user_repo->UpdatePasswordHash(uid, new_password_hash, &err)) {
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger) << "UpdatePasswordHash failed, uid=" << uid << ", err=" << err;
            result.code = 500;
            result.err = "更新密码失败";
            return result;
        }
    }
    result.ok = true;
    return result;
}

Result<model::User> UserServiceImpl::GetUserByMobile(const std::string& mobile,
                                                     const std::string& channel) {
    Result<model::User> result;
    std::string err;

    if (channel == "register") {
        if (m_user_repo->GetUserByMobile(mobile, result.data, &err)) {
            IM_LOG_ERROR(g_logger)
                << "GetUserByMobile failed, mobile=" << mobile << ", err=" << err;
            result.code = 400;
            result.err = "手机号已注册!";
            return result;
        }
    } else if (channel == "forget_account") {
        if (!m_user_repo->GetUserByMobile(mobile, result.data, &err)) {
            IM_LOG_ERROR(g_logger)
                << "GetUserByMobile failed, mobile=" << mobile << ", err=" << err;
            result.code = 400;
            result.err = "手机号未注册!";
            return result;
        }
    }
    result.ok = true;
    return result;
}

Result<model::User> UserServiceImpl::GetUserByEmail(const std::string& email,
                                                    const std::string& channel) {
    Result<model::User> result;
    std::string err;
    if (channel == "register" || channel == "update_email") {
        if (m_user_repo->GetUserByEmail(email, result.data, &err)) {
            if (!err.empty()) {
                IM_LOG_ERROR(g_logger)
                    << "GetUserByEmail failed, email=" << email << ", err=" << err;
                result.code = 500;
                result.err = "查询邮箱失败!";
                return result;
            }
        }
    } else if (channel == "forget_account") {
        if (!m_user_repo->GetUserByEmail(email, result.data, &err)) {
            IM_LOG_ERROR(g_logger) << "GetUserByEmail failed, email=" << email << ", err=" << err;
            result.code = 400;
            result.err = "邮箱未注册!";
            return result;
        }
    }
    result.ok = true;
    return result;
}

Result<void> UserServiceImpl::Offline(const uint64_t id) {
    Result<void> result;
    std::string err;

    if (!m_user_repo->UpdateOfflineStatus(id, &err)) {
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger)
                << "UpdateOfflineStatus failed, user_id=" << id << ", err=" << err;
            result.code = 500;
            result.err = "更新在线状态失败";
            return result;
        }
    }

    result.ok = true;
    return result;
}

Result<std::string> UserServiceImpl::GetUserOnlineStatus(const uint64_t id) {
    Result<std::string> result;
    std::string err;

    if (!m_user_repo->GetOnlineStatus(id, result.data, &err)) {
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger)
                << "GetUserOnlineStatus failed, user_id=" << id << ", err=" << err;
            result.code = 500;
            result.err = "获取用户在线状态失败";
            return result;
        }
    }

    result.ok = true;
    return result;
}

Result<void> UserServiceImpl::SaveConfigInfo(const uint64_t user_id, const std::string& theme_mode,
                                             const std::string& theme_bag_img,
                                             const std::string& theme_color,
                                             const std::string& notify_cue_tone,
                                             const std::string& keyboard_event_notify) {
    Result<void> result;
    std::string err;

    IM::model::UserSettings new_settings;
    new_settings.user_id = user_id;
    new_settings.theme_mode = theme_mode;
    new_settings.theme_bag_img = theme_bag_img;
    new_settings.theme_color = theme_color;
    new_settings.notify_cue_tone = notify_cue_tone;
    new_settings.keyboard_event_notify = keyboard_event_notify;
    if (!m_user_repo->UpsertUserSettings(new_settings, &err)) {
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger)
                << "Upsert new UserSettings failed, user_id=" << user_id << ", err=" << err;
            result.code = 500;
            result.err = "保存用户设置失败";
            return result;
        }
    }
    result.ok = true;
    return result;
}

Result<IM::model::UserSettings> UserServiceImpl::LoadConfigInfo(const uint64_t user_id) {
    Result<IM::model::UserSettings> result;
    std::string err;

    if (!m_user_repo->GetUserSettings(user_id, result.data, &err)) {
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger)
                << "LoadConfigInfo failed, user_id=" << user_id << ", err=" << err;
            result.code = 500;
            result.err = "加载用户设置失败";
            return result;
        }
    }

    result.ok = true;
    return result;
}

Result<dto::UserInfo> UserServiceImpl::LoadUserInfoSimple(const uint64_t uid) {
    Result<dto::UserInfo> result;
    std::string err;

    if (!m_user_repo->GetUserInfoSimple(uid, result.data, &err)) {
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger) << "LoadUserInfoSimple failed, uid=" << uid << ", err=" << err;
            result.code = 404;
            result.err = "加载用户信息失败";
            return result;
        }
    }

    if (!result.data.avatar.empty() && result.data.avatar.length() == 32) {
        bool is_id = true;
        for (char c : result.data.avatar) {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                is_id = false;
                break;
            }
        }
        if (is_id) {
            IM::model::MediaFile media;
            auto gf_res2 = m_media_service->GetMediaFile(result.data.avatar);
            if (gf_res2.ok) {
                result.data.avatar = gf_res2.data.url;
            }
        }
    }

    result.ok = true;
    return result;
}

Result<model::User> UserServiceImpl::Authenticate(const std::string& mobile,
                                                  const std::string& password,
                                                  const std::string& platform) {
    Result<model::User> result;
    std::string err;

    // 密码解密
    std::string decrypted_pwd;
    auto dec_res = IM::util::DecryptPassword(password, decrypted_pwd);
    if (!dec_res.ok) {
        result.code = dec_res.code;
        result.err = dec_res.err;
        return result;
    }

    // 获取用户信息
    if (!m_user_repo->GetUserByMobile(mobile, result.data, &err)) {
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger)
                << "Authenticate GetUserByMobile failed, mobile=" << mobile << ", err=" << err;
            result.code = 500;
            result.err = "获取用户信息失败";
            return result;
        }
        result.code = 404;
        result.err = "手机号或密码错误";
        return result;
    }

    // 检查用户是否被禁用
    if (result.data.is_disabled == 1) {
        result.code = 403;
        result.err = "账户被禁用!";
        return result;
    }

    // 获取用户认证信息
    IM::model::UserAuth ua;
    if (!m_user_repo->GetUserAuthById(result.data.id, ua, &err)) {
        if (!err.empty()) {
            IM_LOG_ERROR(g_logger)
                << "Authenticate GetUserAuthById failed, user_id=" << result.data.id
                << ", err=" << err;
            result.code = 500;
            result.err = "获取用户认证信息失败";
            return result;
        }
        result.code = 404;
        result.err = "手机号或密码错误";
        return result;
    }

    // 验证密码
    if (!IM::util::Password::Verify(decrypted_pwd, ua.password_hash)) {
        result.err = "手机号或密码错误";
        return result;
    }

    result.ok = true;
    return result;
}

Result<void> UserServiceImpl::LogLogin(const Result<model::User>& user_result,
                                       const std::string& platform,
                                       IM::http::HttpSession::ptr session) {
    Result<void> result;
    std::string err;

    IM::model::UserLoginLog log;
    log.user_id = user_result.data.id;
    log.mobile = user_result.data.mobile;
    log.platform = platform;
    log.ip = session->getRemoteAddressString();
    log.user_agent = "UA";
    log.success = user_result.ok ? 1 : 0;
    log.reason = user_result.ok ? "" : user_result.err;
    log.created_at = TimeUtil::NowToS();

    if (!m_user_repo->CreateUserLoginLog(log, &err)) {
        IM_LOG_ERROR(g_logger) << "LogLogin Create UserLoginLog failed, user_id="
                               << user_result.data.id << ", err=" << err;
        result.code = 500;
        result.err = "记录登录日志失败";
        return result;
    }

    result.ok = true;
    return result;
}

Result<void> UserServiceImpl::GoOnline(const uint64_t id) {
    Result<void> result;
    std::string err;

    if (!m_user_repo->UpdateOnlineStatus(id, &err)) {
        IM_LOG_ERROR(g_logger) << "UpdateOnlineStatus failed, user_id=" << id << ", err=" << err;
        result.code = 500;
        result.err = "更新在线状态失败";
        return result;
    }

    result.ok = true;
    return result;
}

Result<model::User> UserServiceImpl::Register(const std::string& nickname,
                                              const std::string& mobile,
                                              const std::string& password,
                                              const std::string& sms_code,
                                              const std::string& platform) {
    Result<model::User> result;
    std::string err;

    auto verifyResult = m_common_service->VerifySmsCode(mobile, sms_code, "register");
    if (!verifyResult.ok) {
        result.code = verifyResult.code;
        result.err = verifyResult.err;
        return result;
    }

    // 密码解密
    std::string decrypted_pwd;
    auto dec_res = IM::util::DecryptPassword(password, decrypted_pwd);
    if (!dec_res.ok) {
        result.code = dec_res.code;
        result.err = dec_res.err;
        return result;
    }

    // 生成密码哈希
    auto ph = IM::util::Password::Hash(decrypted_pwd);
    if (ph.empty()) {
        result.err = "密码哈希生成失败";
        return result;
    }

    // 开启事务
    auto trans = IM::MySQLMgr::GetInstance()->openTransaction(kDBName, false);
    if (!trans) {
        IM_LOG_ERROR(g_logger) << "Register openTransaction failed";
        result.code = 500;
        result.err = "创建账号失败";
        return result;
    }

    // 获取事务绑定的数据库连接
    auto db = trans->getMySQL();
    if (!db) {
        IM_LOG_ERROR(g_logger) << "Register getMySQL failed";
        result.code = 500;
        result.err = "创建账号失败";
        return result;
    }

    // 创建用户
    IM::model::User u;
    u.nickname = nickname;
    u.mobile = mobile;

    if (!m_user_repo->CreateUser(db, u, u.id, &err)) {
        IM_LOG_ERROR(g_logger) << "Register Create user failed, mobile=" << mobile
                               << ", err=" << err;
        trans->rollback();  // 回滚事务
        result.code = 500;
        result.err = "创建用户失败";
        return result;
    }

    // 创建密码认证记录
    IM::model::UserAuth ua;
    ua.user_id = u.id;
    ua.password_hash = ph;

    if (!m_user_repo->CreateUserAuth(db, ua, &err)) {
        IM_LOG_ERROR(g_logger) << "Register Create user_auth failed, user_id=" << u.id
                               << ", err=" << err;
        trans->rollback();  // 回滚事务
        result.code = 500;
        result.err = "创建用户认证信息失败";
        return result;
    }

    if (!trans->commit()) {
        // 提交失败也要回滚，保证数据一致性
        const auto commit_err = db->getErrStr();
        trans->rollback();
        IM_LOG_ERROR(g_logger) << "Register commit transaction failed, mobile=" << mobile
                               << ", err=" << commit_err;
        result.code = 500;
        result.err = "处理好友申请失败";
        return result;
    }

    result.ok = true;
    result.data = std::move(u);
    return result;
}

Result<model::User> UserServiceImpl::Forget(const std::string& mobile,
                                            const std::string& new_password,
                                            const std::string& sms_code) {
    Result<model::User> result;
    std::string err;

    /* 验证短信验证码 */
    auto verifyResult = m_common_service->VerifySmsCode(mobile, sms_code, "forget_account");
    if (!verifyResult.ok) {
        result.code = verifyResult.code;
        result.err = verifyResult.err;
        return result;
    }

    // 密码解密
    std::string decrypted_pwd;
    auto dec_res = IM::util::DecryptPassword(new_password, decrypted_pwd);
    if (!dec_res.ok) {
        result.code = dec_res.code;
        result.err = dec_res.err;
        return result;
    }

    /*检查手机号是否存在*/
    if (!m_user_repo->GetUserByMobile(mobile, result.data, &err)) {
        IM_LOG_ERROR(g_logger) << "Forget GetByMobile failed, mobile=" << mobile << ", err=" << err;
        result.code = 404;
        result.err = "手机号不存在";
        return result;
    }

    /*生成新密码哈希*/
    auto password_hash = IM::util::Password::Hash(decrypted_pwd);
    if (password_hash.empty()) {
        result.err = "密码哈希生成失败";
        return result;
    }

    /*更新用户密码*/
    if (!m_user_repo->UpdatePasswordHash(result.data.id, password_hash, &err)) {
        IM_LOG_ERROR(g_logger) << "Forget UpdatePasswordHash failed, mobile=" << mobile
                               << ", err=" << err;
        result.code = 500;
        result.err = "更新密码失败";
        return result;
    }

    result.ok = true;
    return result;
}

}  // namespace IM::app
