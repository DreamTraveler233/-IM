#include "dao/user_settings_dao.hpp"

#include <mysql/mysql.h>

#include "db/mysql.hpp"
#include "macro.hpp"

namespace CIM::dao {

static const char* kDBName = "default";

bool UserSettingsDAO::Upsert(const UserSettings& settings, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }

    const char* sql =
        "INSERT INTO user_settings (user_id, theme_mode, theme_bag_img, theme_color, "
        "notify_cue_tone, keyboard_event_notify) "
        "VALUES (?, ?, ?, ?, ?, ?) "
        "ON DUPLICATE KEY UPDATE "
        "theme_mode = VALUES(theme_mode), "
        "theme_bag_img = VALUES(theme_bag_img), "
        "theme_color = VALUES(theme_color), "
        "notify_cue_tone = VALUES(notify_cue_tone), "
        "keyboard_event_notify = VALUES(keyboard_event_notify), "
        "updated_at = NOW()";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindUint64(1, settings.user_id);
    stmt->bindString(2, settings.theme_mode);
    if (!settings.theme_bag_img.empty())
        stmt->bindString(3, settings.theme_bag_img);
    else
        stmt->bindNull(3);
    stmt->bindString(4, settings.theme_color);
    stmt->bindString(5, settings.notify_cue_tone);
    stmt->bindString(6, settings.keyboard_event_notify);

    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool UserSettingsDAO::GetByUserId(uint64_t user_id, UserSettings& out) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) return false;

    const char* sql =
        "SELECT user_id, theme_mode, theme_bag_img, theme_color, notify_cue_tone, "
        "keyboard_event_notify, created_at, updated_at "
        "FROM user_settings WHERE user_id = ? LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) return false;
    stmt->bindUint64(1, user_id);
    auto res = stmt->query();
    if (!res) return false;
    if (!res->next()) return false;

    out.user_id = static_cast<uint64_t>(res->getUint64(0));
    out.theme_mode = res->getString(1);
    out.theme_bag_img = res->isNull(2) ? std::string() : res->getString(2);
    out.theme_color = res->getString(3);
    out.notify_cue_tone = res->getString(4);
    out.keyboard_event_notify = res->getString(5);
    out.created_at = res->getTime(6);
    out.updated_at = res->getTime(7);
    return true;
}

bool UserSettingsDAO::DeleteByUserId(uint64_t user_id, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }

    const char* sql = "DELETE FROM user_settings WHERE user_id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindUint64(1, user_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

}  // namespace CIM::dao