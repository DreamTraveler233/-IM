#include "infra/repository/user_repository_impl.hpp"

#include "base/macro.hpp"

namespace IM::infra::repository {

static constexpr const char* kDBName = "default";
static auto g_logger = IM_LOG_NAME("system");

UserRepositoryImpl::UserRepositoryImpl(std::shared_ptr<IM::MySQLManager> db_manager)
    : m_db_manager(std::move(db_manager)) {}

bool UserRepositoryImpl::CreateUser(const std::shared_ptr<IM::MySQL>& db, const model::User& u,
                                    uint64_t& out_id, std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "INSERT INTO im_user (mobile, email, nickname, avatar, avatar_media_id, motto, birthday, "
        "gender, "
        "online_status, last_online_at, is_qiye, is_robot, is_disabled, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, NOW(), NOW())";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindString(1, u.mobile);
    if (!u.email.empty())
        stmt->bindString(2, u.email);
    else
        stmt->bindNull(2);
    stmt->bindString(3, u.nickname);
    if (!u.avatar.empty())
        stmt->bindString(4, u.avatar);
    else
        stmt->bindNull(4);
    if (!u.avatar_media_id.empty())
        stmt->bindString(5, u.avatar_media_id);
    else
        stmt->bindNull(5);
    if (!u.motto.empty())
        stmt->bindString(6, u.motto);
    else
        stmt->bindNull(6);
    if (!u.birthday.empty())
        stmt->bindString(7, u.birthday);
    else
        stmt->bindNull(7);
    stmt->bindUint8(8, u.gender);
    stmt->bindString(9, u.online_status);
    if (u.last_online_at != 0)
        stmt->bindTime(10, u.last_online_at);
    else
        stmt->bindNull(10);
    stmt->bindUint8(11, u.is_qiye);
    stmt->bindUint8(12, u.is_robot);
    stmt->bindUint8(13, u.is_disabled);

    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    out_id = stmt->getLastInsertId();
    return true;
}

bool UserRepositoryImpl::GetUserByMobile(const std::string& mobile, model::User& out,
                                         std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT id, mobile, email, nickname, avatar, avatar_media_id, motto, DATE_FORMAT(birthday, "
        "'%Y-%m-%d') as "
        "birthday, gender, online_status, last_online_at, is_qiye, is_robot, is_disabled, "
        "created_at, updated_at FROM im_user WHERE mobile = ? LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindString(1, mobile);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }

    if (!res->next()) {
        if (err) *err = "no record found";
        return false;
    }

    out.id = res->getUint64(0);
    out.mobile = res->isNull(1) ? std::string() : res->getString(1);
    out.email = res->isNull(2) ? std::string() : res->getString(2);
    out.nickname = res->isNull(3) ? std::string() : res->getString(3);
    out.avatar = res->isNull(4) ? std::string() : res->getString(4);
    out.avatar_media_id = res->isNull(5) ? std::string() : res->getString(5);
    out.motto = res->isNull(6) ? std::string() : res->getString(6);
    out.birthday = res->isNull(7) ? std::string() : res->getString(7);
    out.gender = res->getUint8(8);
    out.online_status = res->isNull(9) ? "N" : res->getString(9);
    out.last_online_at = res->isNull(10) ? 0 : res->getTime(10);
    out.is_qiye = res->isNull(11) ? 0 : res->getUint8(11);
    out.is_robot = res->isNull(12) ? 0 : res->getUint8(12);
    out.is_disabled = res->isNull(13) ? 0 : res->getUint8(13);
    out.created_at = res->getTime(14);
    out.updated_at = res->getTime(15);

    return true;
}

bool UserRepositoryImpl::GetUserByEmail(const std::string& email, model::User& out,
                                        std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT id, mobile, email, nickname, avatar, avatar_media_id, motto, DATE_FORMAT(birthday, "
        "'%Y-%m-%d') as "
        "birthday, gender, online_status, last_online_at, is_qiye, is_robot, is_disabled, "
        "created_at, updated_at FROM im_user WHERE email = ? LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindString(1, email);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }

    if (!res->next()) {
        if (err) *err = "no record found";
        return false;
    }

    out.id = res->getUint64(0);
    out.mobile = res->isNull(1) ? std::string() : res->getString(1);
    out.email = res->isNull(2) ? std::string() : res->getString(2);
    out.nickname = res->isNull(3) ? std::string() : res->getString(3);
    out.avatar = res->isNull(4) ? std::string() : res->getString(4);
    out.avatar_media_id = res->isNull(5) ? std::string() : res->getString(5);
    out.motto = res->isNull(6) ? std::string() : res->getString(6);
    out.birthday = res->isNull(7) ? std::string() : res->getString(7);
    out.gender = res->getUint8(8);
    out.online_status = res->isNull(9) ? "N" : res->getString(9);
    out.last_online_at = res->isNull(10) ? 0 : res->getTime(10);
    out.is_qiye = res->isNull(11) ? 0 : res->getUint8(11);
    out.is_robot = res->isNull(12) ? 0 : res->getUint8(12);
    out.is_disabled = res->isNull(13) ? 0 : res->getUint8(13);
    out.created_at = res->getTime(14);
    out.updated_at = res->getTime(15);

    return true;
}

bool UserRepositoryImpl::GetUserById(const uint64_t id, model::User& out, std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT id, mobile, email, nickname, avatar, avatar_media_id, motto, DATE_FORMAT(birthday, "
        "'%Y-%m-%d') as birthday, gender, online_status, last_online_at, is_robot, is_qiye, "
        "is_disabled, created_at, updated_at FROM im_user WHERE id = ? LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }

    if (!res->next()) {
        if (err) *err = "no record found";
        return false;
    }

    out.id = res->getUint64(0);
    out.mobile = res->isNull(1) ? std::string() : res->getString(1);
    out.email = res->isNull(2) ? std::string() : res->getString(2);
    out.nickname = res->isNull(3) ? std::string() : res->getString(3);
    out.avatar = res->isNull(4) ? std::string() : res->getString(4);
    out.avatar_media_id = res->isNull(5) ? std::string() : res->getString(5);
    out.motto = res->isNull(6) ? std::string() : res->getString(6);
    out.birthday = res->isNull(7) ? std::string() : res->getString(7);
    out.gender = res->getUint8(8);
    out.online_status = res->isNull(9) ? std::string("N") : res->getString(9);
    out.last_online_at = res->isNull(10) ? 0 : res->getTime(10);
    out.is_robot = res->isNull(11) ? 0 : res->getUint8(11);
    out.is_qiye = res->isNull(12) ? 0 : res->getUint8(12);
    out.is_disabled = res->isNull(13) ? 0 : res->getUint8(13);
    out.created_at = res->getTime(14);
    out.updated_at = res->getTime(15);

    return true;
}

bool UserRepositoryImpl::UpdateUserInfo(const uint64_t id, const std::string& nickname,
                                        const std::string& avatar,
                                        const std::string& avatar_media_id,
                                        const std::string& motto, const uint8_t gender,
                                        const std::string& birthday, std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }

    const char* sql =
        "UPDATE im_user SET nickname = COALESCE(NULLIF(?, ''), nickname), avatar = ?, "
        "avatar_media_id = ?, motto = NULLIF(?, ''), gender = ?, "
        "birthday = ?, "
        "updated_at = NOW() WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    // 始终绑定 nickname，但 SQL 会在为空时回退到现有值
    stmt->bindString(1, nickname);
    if (!avatar.empty())
        stmt->bindString(2, avatar);
    else
        stmt->bindNull(2);
    if (!avatar_media_id.empty())
        stmt->bindString(3, avatar_media_id);
    else
        stmt->bindNull(3);
    // 将 motto 绑定为字符串，SQL 的 NULLIF 会在空字符串时将其置为 NULL
    stmt->bindString(4, motto);
    stmt->bindUint8(5, gender);
    if (!birthday.empty())
        stmt->bindString(6, birthday);
    else
        stmt->bindNull(6);
    stmt->bindUint64(7, id);
    IM_LOG_DEBUG(g_logger) << "UserDAO::UpdateUserInfo bind values: id=" << id << " nickname='"
                           << nickname << "' avatar='" << avatar
                           << "' avatar_media_id='" << avatar_media_id << "' motto='" << motto
                           << "' gender=" << (int)gender << " birthday='" << birthday << "'";
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool UserRepositoryImpl::UpdateUserInfoWithConn(const std::shared_ptr<IM::MySQL>& db,
                                               const uint64_t id, const std::string& nickname,
                                               const std::string& avatar,
                                               const std::string& avatar_media_id,
                                               const std::string& motto, const uint8_t gender,
                                               const std::string& birthday, std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }

    const char* sql =
        "UPDATE im_user SET nickname = COALESCE(NULLIF(?, ''), nickname), avatar = ?, "
        "avatar_media_id = ?, motto = NULLIF(?, ''), gender = ?, "
        "birthday = ?, "
        "updated_at = NOW() WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindString(1, nickname);
    if (!avatar.empty())
        stmt->bindString(2, avatar);
    else
        stmt->bindNull(2);
    if (!avatar_media_id.empty())
        stmt->bindString(3, avatar_media_id);
    else
        stmt->bindNull(3);
    stmt->bindString(4, motto);
    stmt->bindUint8(5, gender);
    if (!birthday.empty())
        stmt->bindString(6, birthday);
    else
        stmt->bindNull(6);
    stmt->bindUint64(7, id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool UserRepositoryImpl::UpdateMobile(const uint64_t id, const std::string& new_mobile,
                                      std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql = "UPDATE im_user SET mobile = ?, updated_at = NOW() WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindString(1, new_mobile);
    stmt->bindUint64(2, id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool UserRepositoryImpl::UpdateEmail(const uint64_t id, const std::string& new_email,
                                     std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql = "UPDATE im_user SET email = ?, updated_at = NOW() WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindString(1, new_email);
    stmt->bindUint64(2, id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool UserRepositoryImpl::UpdateOnlineStatus(const uint64_t id, std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql = "UPDATE im_user SET online_status = ? WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindString(1, "Y");
    stmt->bindUint64(2, id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool UserRepositoryImpl::UpdateOfflineStatus(const uint64_t id, std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql = "UPDATE im_user SET online_status = ?, last_online_at = NOW() WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = db->getErrStr();
        return false;
    }
    stmt->bindString(1, "N");
    stmt->bindUint64(2, id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool UserRepositoryImpl::GetOnlineStatus(const uint64_t id, std::string& out_status,
                                         std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql = "SELECT online_status FROM im_user WHERE id = ? LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = db->getErrStr();
        return false;
    }
    stmt->bindUint64(1, id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }

    if (!res->next()) {
        if (err) *err = "no record found";
        return false;
    }

    out_status = res->isNull(0) ? std::string("N") : res->getString(0);

    return true;
}

bool UserRepositoryImpl::GetUserInfoSimple(const uint64_t uid, dto::UserInfo& out,
                                           std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT id, nickname, avatar, motto, gender, is_qiye, mobile, email "
        "FROM im_user WHERE id = ? LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, uid);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }

    if (!res->next()) {
        if (err) *err = "no record found";
        return false;
    }

    out.uid = res->isNull(0) ? 0 : res->getUint64(0);
    out.nickname = res->isNull(1) ? std::string() : res->getString(1);
    out.avatar = res->isNull(2) ? std::string() : res->getString(2);
    out.motto = res->isNull(3) ? std::string() : res->getString(3);
    out.gender = res->isNull(4) ? 0 : res->getUint8(4);
    out.is_qiye = res->isNull(5) ? false : (res->getUint8(5) != 0);
    out.mobile = res->isNull(6) ? std::string() : res->getString(6);
    out.email = res->isNull(7) ? std::string() : res->getString(7);

    return true;
}

bool UserRepositoryImpl::CreateUserAuth(const std::shared_ptr<IM::MySQL>& db,
                                        const model::UserAuth& ua, std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "INSERT INTO im_user_auth (user_id, password_hash, password_algo, password_version "
        ", last_reset_at, created_at, updated_at) VALUES (?, ?, ?, ?, ?, NOW(), NOW())";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, ua.user_id);
    stmt->bindString(2, ua.password_hash);
    stmt->bindString(3, ua.password_algo);
    stmt->bindInt16(4, ua.password_version);
    if (ua.last_reset_at != 0)
        stmt->bindTime(5, ua.last_reset_at);
    else
        stmt->bindNull(5);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool UserRepositoryImpl::GetUserAuthById(const uint64_t user_id, model::UserAuth& out,
                                         std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT user_id, password_hash, password_algo, password_version, last_reset_at, "
        "created_at, updated_at "
        "FROM im_user_auth WHERE user_id = ? LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, user_id);
    auto res = stmt->query();
    if (!res || !res->next()) {
        if (err) *err = "query failed";
        return false;
    }

    out.user_id = res->getUint64(0);
    out.password_hash = res->isNull(1) ? std::string() : res->getString(1);
    out.password_algo = res->isNull(2) ? std::string("PBKDF2-HMAC-SHA256") : res->getString(2);
    out.password_version = res->isNull(3) ? 1 : res->getInt16(3);
    out.last_reset_at = res->isNull(4) ? 0 : res->getTime(4);
    out.created_at = res->getTime(5);
    out.updated_at = res->getTime(6);

    return true;
}

bool UserRepositoryImpl::UpdatePasswordHash(const uint64_t user_id,
                                            const std::string& new_password_hash,
                                            std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "UPDATE im_user_auth SET password_hash = ?, updated_at = NOW() WHERE user_id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = db->getErrStr();
        return false;
    }
    stmt->bindString(1, new_password_hash);
    stmt->bindUint64(2, user_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool UserRepositoryImpl::CreateUserLoginLog(const model::UserLoginLog& log, std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "INSERT INTO im_auth_login_log (user_id, mobile, platform, ip, address, user_agent, "
        "success, reason, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, NOW())";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    if (log.user_id != 0)
        stmt->bindUint64(1, log.user_id);
    else
        stmt->bindNull(1);
    if (!log.mobile.empty())
        stmt->bindString(2, log.mobile);
    else
        stmt->bindNull(2);
    stmt->bindString(3, log.platform);
    stmt->bindString(4, log.ip);
    if (!log.address.empty())
        stmt->bindString(5, log.address);
    else
        stmt->bindNull(5);
    if (!log.user_agent.empty())
        stmt->bindString(6, log.user_agent);
    else
        stmt->bindNull(6);
    stmt->bindInt32(7, log.success);
    if (!log.reason.empty())
        stmt->bindString(8, log.reason);
    else
        stmt->bindNull(8);

    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool UserRepositoryImpl::UpsertUserSettings(const model::UserSettings& settings, std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }

    const char* sql =
        "INSERT INTO im_user_setting (user_id, theme_mode, theme_bag_img, theme_color, "
        "notify_cue_tone, keyboard_event_notify, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, NOW(), NOW()) "
        "ON DUPLICATE KEY UPDATE "
        "theme_mode = VALUES(theme_mode), "
        "theme_bag_img = VALUES(theme_bag_img), "
        "theme_color = VALUES(theme_color), "
        "notify_cue_tone = VALUES(notify_cue_tone), "
        "keyboard_event_notify = VALUES(keyboard_event_notify), "
        "updated_at = NOW()";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, settings.user_id);
    stmt->bindString(2, settings.theme_mode);
    stmt->bindString(3, settings.theme_bag_img);
    stmt->bindString(4, settings.theme_color);
    stmt->bindString(5, settings.notify_cue_tone);
    stmt->bindString(6, settings.keyboard_event_notify);

    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool UserRepositoryImpl::GetUserSettings(const uint64_t user_id, model::UserSettings& out,
                                         std::string* err) {
    auto db = m_db_manager->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT user_id, theme_mode, theme_bag_img, theme_color, notify_cue_tone, "
        "keyboard_event_notify FROM im_user_setting WHERE user_id = ? LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, user_id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    while (res->next()) {
        out.theme_mode = res->isNull(1) ? std::string() : res->getString(1);
        out.theme_bag_img = res->isNull(2) ? std::string() : res->getString(2);
        out.theme_color = res->isNull(3) ? std::string() : res->getString(3);
        out.notify_cue_tone = res->isNull(4) ? std::string() : res->getString(4);
        out.keyboard_event_notify = res->isNull(5) ? std::string() : res->getString(5);
        return true;
    }
    return false;
}

}  // namespace IM::infra