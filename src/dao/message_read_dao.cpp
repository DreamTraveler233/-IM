#include "dao/message_read_dao.hpp"

#include "db/mysql.hpp"

namespace CIM::dao {

static constexpr const char* kDBName = "default";

bool MessageReadDao::MarkRead(const std::string& msg_id, const uint64_t user_id, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "INSERT IGNORE INTO im_message_read (msg_id,user_id,read_at) VALUES (?,?,NOW())";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindString(1, msg_id);
    stmt->bindUint64(2, user_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}
}  // namespace CIM::dao
