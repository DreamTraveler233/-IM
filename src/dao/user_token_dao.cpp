#include "dao/user_token_dao.hpp"

#include <mysql/mysql.h>

#include "db/mysql.hpp"
#include "macro.hpp"

namespace CIM::dao {

static const char* kDBName = "default";

bool UserTokenDAO::Create(const UserToken& token, uint64_t& out_id, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }

    const char* sql =
        "INSERT INTO user_tokens (user_id, access_token, platform, client_ip, expires_at) "
        "VALUES (?, ?, ?, ?, ?)";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindUint64(1, token.user_id);
    stmt->bindString(2, token.access_token);
    stmt->bindString(3, token.platform);
    if (!token.client_ip.empty())
        stmt->bindString(4, token.client_ip);
    else
        stmt->bindNull(4);
    stmt->bindTime(5, token.expires_at);

    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    out_id = static_cast<uint64_t>(stmt->getLastInsertId());
    return true;
}

bool UserTokenDAO::GetByToken(const std::string& access_token, UserToken& out) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) return false;

    const char* sql =
        "SELECT id, user_id, access_token, platform, client_ip, expires_at, revoked_at, created_at "
        "FROM user_tokens WHERE access_token = ? AND (revoked_at IS NULL OR revoked_at = 0) LIMIT "
        "1";
    auto stmt = db->prepare(sql);
    if (!stmt) return false;
    stmt->bindString(1, access_token);
    auto res = stmt->query();
    if (!res) return false;
    if (!res->next()) return false;

    out.id = static_cast<uint64_t>(res->getUint64(0));
    out.user_id = static_cast<uint64_t>(res->getUint64(1));
    out.access_token = res->getString(2);
    out.platform = res->getString(3);
    out.client_ip = res->isNull(4) ? std::string() : res->getString(4);
    out.expires_at = res->getTime(5);
    out.revoked_at = res->isNull(6) ? 0 : res->getTime(6);
    out.created_at = res->getTime(7);
    return true;
}

bool UserTokenDAO::GetByUserId(uint64_t user_id, std::vector<UserToken>& out) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) return false;

    const char* sql =
        "SELECT id, user_id, access_token, platform, client_ip, expires_at, revoked_at, created_at "
        "FROM user_tokens WHERE user_id = ? AND (revoked_at IS NULL OR revoked_at = 0) "
        "ORDER BY created_at DESC";
    auto stmt = db->prepare(sql);
    if (!stmt) return false;
    stmt->bindUint64(1, user_id);
    auto res = stmt->query();
    if (!res) return false;

    out.clear();
    while (res->next()) {
        UserToken token;
        token.id = static_cast<uint64_t>(res->getUint64(0));
        token.user_id = static_cast<uint64_t>(res->getUint64(1));
        token.access_token = res->getString(2);
        token.platform = res->getString(3);
        token.client_ip = res->isNull(4) ? std::string() : res->getString(4);
        token.expires_at = res->getTime(5);
        token.revoked_at = res->isNull(6) ? 0 : res->getTime(6);
        token.created_at = res->getTime(7);
        out.push_back(std::move(token));
    }
    return true;
}

bool UserTokenDAO::RevokeToken(const std::string& access_token, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }

    const char* sql = "UPDATE user_tokens SET revoked_at = NOW() WHERE access_token = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindString(1, access_token);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool UserTokenDAO::RevokeAllUserTokens(uint64_t user_id, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }

    const char* sql =
        "UPDATE user_tokens SET revoked_at = NOW() WHERE user_id = ? AND (revoked_at IS NULL OR "
        "revoked_at = 0)";
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

bool UserTokenDAO::CleanupExpiredTokens(std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }

    const char* sql =
        "DELETE FROM user_tokens WHERE expires_at < NOW() OR (revoked_at IS NOT NULL AND "
        "revoked_at < NOW())";
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