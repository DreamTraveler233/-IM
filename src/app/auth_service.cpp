#include "app/auth_service.hpp"

#include "common/common.hpp"
#include "other/crypto_module.hpp"
#include "dao/user_auth_dao.hpp"
#include "db/mysql.hpp"
#include "base/macro.hpp"
#include "util/password.hpp"
#include "util/util.hpp"

namespace CIM::app {

static auto g_logger = CIM_LOG_NAME("root");
static constexpr const char* kDBName = "default";

UserResult AuthService::Authenticate(const std::string& mobile, const std::string& password,
                                     const std::string& platform) {
    UserResult result;
    std::string err;

    // 密码解密
    std::string decrypted_pwd;
    auto dec_res = CIM::DecryptPassword(password, decrypted_pwd);
    if (!dec_res.ok) {
        result.code = dec_res.code;
        result.err = dec_res.err;
        return result;
    }

    // 获取用户信息
    if (!CIM::dao::UserDAO::GetByMobile(mobile, result.data, &err)) {
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger)
                << "Authenticate GetByMobile failed, mobile=" << mobile << ", err=" << err;
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
    CIM::dao::UserAuth ua;
    if (!CIM::dao::UserAuthDao::GetByUserId(result.data.id, ua, &err)) {
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger)
                << "Authenticate GetByUserId failed, user_id=" << result.data.id << ", err=" << err;
            result.code = 500;
            result.err = "获取用户认证信息失败";
            return result;
        }
        result.code = 404;
        result.err = "手机号或密码错误";
        return result;
    }

    // 验证密码
    if (!CIM::util::Password::Verify(decrypted_pwd, ua.password_hash)) {
        result.err = "手机号或密码错误";
        return result;
    }

    result.ok = true;
    return result;
}

VoidResult AuthService::LogLogin(const UserResult& user_result, const std::string& platform,
                                 CIM::http::HttpSession::ptr session) {
    VoidResult result;
    std::string err;

    CIM::dao::UserLoginLog log;
    log.user_id = user_result.data.id;
    log.mobile = user_result.data.mobile;
    log.platform = platform;
    log.ip = session->getRemoteAddressString();
    log.user_agent = "UA";
    log.success = user_result.ok ? 1 : 0;
    log.reason = user_result.ok ? "" : user_result.err;
    log.created_at = TimeUtil::NowToS();

    if (!CIM::dao::UserLoginLogDAO::Create(log, &err)) {
        CIM_LOG_ERROR(g_logger) << "LogLogin Create UserLoginLog failed, user_id="
                                << user_result.data.id << ", err=" << err;
        result.code = 500;
        result.err = "记录登录日志失败";
        return result;
    }

    result.ok = true;
    return result;
}

VoidResult AuthService::GoOnline(const uint64_t id) {
    VoidResult result;
    std::string err;

    if (!CIM::dao::UserDAO::UpdateOnlineStatus(id, &err)) {
        CIM_LOG_ERROR(g_logger) << "UpdateOnlineStatus failed, user_id=" << id << ", err=" << err;
        result.code = 500;
        result.err = "更新在线状态失败";
        return result;
    }

    result.ok = true;
    return result;
}

UserResult AuthService::Register(const std::string& nickname, const std::string& mobile,
                                 const std::string& password, const std::string& platform) {
    UserResult result;
    std::string err;

    // 密码解密
    std::string decrypted_pwd;
    auto dec_res = CIM::DecryptPassword(password, decrypted_pwd);
    if (!dec_res.ok) {
        result.code = dec_res.code;
        result.err = dec_res.err;
        return result;
    }

    // 生成密码哈希
    auto ph = CIM::util::Password::Hash(decrypted_pwd);
    if (ph.empty()) {
        result.err = "密码哈希生成失败";
        return result;
    }

    // 开启事务
    auto trans = CIM::MySQLMgr::GetInstance()->openTransaction(kDBName, false);
    if (!trans) {
        CIM_LOG_ERROR(g_logger) << "Register openTransaction failed";
        result.code = 500;
        result.err = "创建账号失败";
        return result;
    }

    // 获取事务绑定的数据库连接
    auto db = trans->getMySQL();
    if (!db) {
        CIM_LOG_ERROR(g_logger) << "Register getMySQL failed";
        result.code = 500;
        result.err = "创建账号失败";
        return result;
    }

    // 创建用户
    CIM::dao::User u;
    u.nickname = nickname;
    u.mobile = mobile;

    if (!CIM::dao::UserDAO::Create(db, u, u.id, &err)) {
        CIM_LOG_ERROR(g_logger) << "Register Create user failed, mobile=" << mobile
                                << ", err=" << err;
        trans->rollback();  // 回滚事务
        result.code = 500;
        result.err = "创建用户失败";
        return result;
    }

    // 创建密码认证记录
    CIM::dao::UserAuth ua;
    ua.user_id = u.id;
    ua.password_hash = ph;

    if (!CIM::dao::UserAuthDao::Create(db, ua, &err)) {
        CIM_LOG_ERROR(g_logger) << "Register Create user_auth failed, user_id=" << u.id
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
        CIM_LOG_ERROR(g_logger) << "Register commit transaction failed, mobile=" << mobile
                                << ", err=" << commit_err;
        result.code = 500;
        result.err = "处理好友申请失败";
        return result;
    }

    result.ok = true;
    result.data = std::move(u);
    return result;
}

UserResult AuthService::Forget(const std::string& mobile, const std::string& new_password) {
    UserResult result;
    std::string err;

    // 密码解密
    std::string decrypted_pwd;
    auto dec_res = CIM::DecryptPassword(new_password, decrypted_pwd);
    if (!dec_res.ok) {
        result.code = dec_res.code;
        result.err = dec_res.err;
        return result;
    }

    /*检查手机号是否存在*/
    if (!CIM::dao::UserDAO::GetByMobile(mobile, result.data, &err)) {
        CIM_LOG_ERROR(g_logger) << "Forget GetByMobile failed, mobile=" << mobile
                                << ", err=" << err;
        result.code = 404;
        result.err = "手机号不存在";
        return result;
    }

    /*生成新密码哈希*/
    auto password_hash = CIM::util::Password::Hash(decrypted_pwd);
    if (password_hash.empty()) {
        result.err = "密码哈希生成失败";
        return result;
    }

    /*更新用户密码*/
    if (!CIM::dao::UserAuthDao::UpdatePasswordHash(result.data.id, password_hash, &err)) {
        CIM_LOG_ERROR(g_logger) << "Forget UpdatePasswordHash failed, mobile=" << mobile
                                << ", err=" << err;
        result.code = 500;
        result.err = "更新密码失败";
        return result;
    }

    result.ok = true;
    return result;
}

}  // namespace CIM::app
