#include "dao/user_settings_dao.hpp"

#include <mysql/mysql.h>

#include "db/mysql.hpp"
#include "base/macro.hpp"

namespace CIM::dao {

static constexpr const char* kDBName = "default";

bool UserSettingsDAO::Upsert(const UserSettings& settings, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
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

bool UserSettingsDAO::GetConfigInfo(uint64_t user_id, ConfigInfo& out, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
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
}  // namespace CIM::dao