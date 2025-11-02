#include "dao/sms_code_dao.hpp"

#include <mysql/mysql.h>

#include "dao/sms_code_dao.hpp"
#include "db/mysql.hpp"
#include "macro.hpp"
#include "sms_code_dao.hpp"

namespace CIM::dao {

static const char* kDBName = "default";

bool SmsCodeDAO::Create(const SmsCode& code, uint64_t& out_id, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }

    const char* sql =
        "INSERT INTO sms_verification_codes (mobile, code, channel, expired_at, send_ip) "
        "VALUES (?, ?, ?, ?, ?)";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindString(1, code.mobile);
    stmt->bindString(2, code.sms_code);
    stmt->bindString(3, code.channel);
    stmt->bindTime(4, code.expired_at);
    if (!code.send_ip.empty())
        stmt->bindString(5, code.send_ip);
    else
        stmt->bindNull(5);

    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    out_id = static_cast<uint64_t>(stmt->getLastInsertId());
    return true;
}

bool SmsCodeDAO::Verify(const std::string& mobile, const std::string& code,
                        const std::string& channel, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }

    const char* sql =
        "SELECT id FROM sms_verification_codes "
        "WHERE mobile = ? AND code = ? AND channel = ? AND status = 0 AND expired_at > NOW() "
        "ORDER BY created_at DESC LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindString(1, mobile);
    stmt->bindString(2, code);
    stmt->bindString(3, channel);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    if (!res->next()) {
        if (err) *err = "verification code not found or expired";
        return false;
    }

    uint64_t id = static_cast<uint64_t>(res->getUint64(0));
    return MarkAsUsed(id, err);
}

bool SmsCodeDAO::MarkAsUsed(uint64_t id, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }

    const char* sql = "UPDATE sms_verification_codes SET status = 1, used_at = NOW() WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindUint64(1, id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool SmsCodeDAO::MarkExpiredAsInvalid(std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql =
        "UPDATE sms_verification_codes SET status = 2 WHERE expired_at < NOW() AND status = 0";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool SmsCodeDAO::DeleteInvalidCodes(std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }

    const char* sql = "DELETE FROM sms_verification_codes WHERE status = 2";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

}  // namespace CIM::dao