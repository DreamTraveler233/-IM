#include "dao/user_login_log_dao.hpp"

#include <mysql/mysql.h>

#include "db/mysql.hpp"
#include "macro.hpp"

namespace CIM::dao {

static const char* kDBName = "default";

bool UserLoginLogDAO::Create(const UserLoginLog& log, uint64_t& out_id, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }

    const char* sql =
        "INSERT INTO user_login_logs (user_id, platform, login_at, status) "
        "VALUES (?, ?, ?, ?)";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindUint64(1, log.user_id);
    stmt->bindString(2, log.platform);
    stmt->bindTime(3, log.login_at);
    stmt->bindInt32(4, log.status);

    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    out_id = static_cast<uint64_t>(stmt->getLastInsertId());
    return true;
}

}  // namespace CIM::dao