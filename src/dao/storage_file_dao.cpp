#include "dao/storage_file_dao.hpp"

#include <mysql/mysql.h>

#include "db/mysql.hpp"
#include "macro.hpp"

namespace CIM::dao {

static const char* kDBName = "default";

bool StorageFileDAO::Create(const StorageFile& file, uint64_t& out_id, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }

    const char* sql =
        "INSERT INTO storage_files (user_id, drive, file_name, file_ext, mime_type, file_size, "
        "file_path, md5_hash, sha1_hash) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    if (file.user_id > 0)
        stmt->bindUint64(1, file.user_id);
    else
        stmt->bindNull(1);
    stmt->bindInt32(2, file.drive);
    stmt->bindString(3, file.file_name);
    if (!file.file_ext.empty())
        stmt->bindString(4, file.file_ext);
    else
        stmt->bindNull(4);
    if (!file.mime_type.empty())
        stmt->bindString(5, file.mime_type);
    else
        stmt->bindNull(5);
    stmt->bindUint64(6, file.file_size);
    stmt->bindString(7, file.file_path);
    if (!file.md5_hash.empty())
        stmt->bindString(8, file.md5_hash);
    else
        stmt->bindNull(8);
    if (!file.sha1_hash.empty())
        stmt->bindString(9, file.sha1_hash);
    else
        stmt->bindNull(9);

    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    out_id = static_cast<uint64_t>(stmt->getLastInsertId());
    return true;
}

bool StorageFileDAO::GetById(uint64_t id, StorageFile& out) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) return false;

    const char* sql =
        "SELECT id, user_id, drive, file_name, file_ext, mime_type, file_size, file_path, "
        "md5_hash, sha1_hash, status, created_at, deleted_at "
        "FROM storage_files WHERE id = ? AND status = 1 LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) return false;
    stmt->bindUint64(1, id);
    auto res = stmt->query();
    if (!res) return false;
    if (!res->next()) return false;

    out.id = static_cast<uint64_t>(res->getUint64(0));
    out.user_id = res->isNull(1) ? 0 : static_cast<uint64_t>(res->getUint64(1));
    out.drive = static_cast<int32_t>(res->getInt32(2));
    out.file_name = res->getString(3);
    out.file_ext = res->isNull(4) ? std::string() : res->getString(4);
    out.mime_type = res->isNull(5) ? std::string() : res->getString(5);
    out.file_size = static_cast<uint64_t>(res->getUint64(6));
    out.file_path = res->getString(7);
    out.md5_hash = res->isNull(8) ? std::string() : res->getString(8);
    out.sha1_hash = res->isNull(9) ? std::string() : res->getString(9);
    out.status = static_cast<int32_t>(res->getInt32(10));
    out.created_at = res->getTime(11);
    out.deleted_at = res->isNull(12) ? 0 : res->getTime(12);
    return true;
}

bool StorageFileDAO::GetByMd5(const std::string& md5_hash, StorageFile& out) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) return false;

    const char* sql =
        "SELECT id, user_id, drive, file_name, file_ext, mime_type, file_size, file_path, "
        "md5_hash, sha1_hash, status, created_at, deleted_at "
        "FROM storage_files WHERE md5_hash = ? AND status = 1 LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) return false;
    stmt->bindString(1, md5_hash);
    auto res = stmt->query();
    if (!res) return false;
    if (!res->next()) return false;

    out.id = static_cast<uint64_t>(res->getUint64(0));
    out.user_id = res->isNull(1) ? 0 : static_cast<uint64_t>(res->getUint64(1));
    out.drive = static_cast<int32_t>(res->getInt32(2));
    out.file_name = res->getString(3);
    out.file_ext = res->isNull(4) ? std::string() : res->getString(4);
    out.mime_type = res->isNull(5) ? std::string() : res->getString(5);
    out.file_size = static_cast<uint64_t>(res->getUint64(6));
    out.file_path = res->getString(7);
    out.md5_hash = res->isNull(8) ? std::string() : res->getString(8);
    out.sha1_hash = res->isNull(9) ? std::string() : res->getString(9);
    out.status = static_cast<int32_t>(res->getInt32(10));
    out.created_at = res->getTime(11);
    out.deleted_at = res->isNull(12) ? 0 : res->getTime(12);
    return true;
}

bool StorageFileDAO::GetByUserId(uint64_t user_id, std::vector<StorageFile>& out, int limit) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) return false;

    const char* sql =
        "SELECT id, user_id, drive, file_name, file_ext, mime_type, file_size, file_path, "
        "md5_hash, sha1_hash, status, created_at, deleted_at "
        "FROM storage_files WHERE user_id = ? AND status = 1 "
        "ORDER BY created_at DESC LIMIT ?";
    auto stmt = db->prepare(sql);
    if (!stmt) return false;
    stmt->bindUint64(1, user_id);
    stmt->bindInt32(2, limit);
    auto res = stmt->query();
    if (!res) return false;

    out.clear();
    while (res->next()) {
        StorageFile file;
        file.id = static_cast<uint64_t>(res->getUint64(0));
        file.user_id = res->isNull(1) ? 0 : static_cast<uint64_t>(res->getUint64(1));
        file.drive = static_cast<int32_t>(res->getInt32(2));
        file.file_name = res->getString(3);
        file.file_ext = res->isNull(4) ? std::string() : res->getString(4);
        file.mime_type = res->isNull(5) ? std::string() : res->getString(5);
        file.file_size = static_cast<uint64_t>(res->getUint64(6));
        file.file_path = res->getString(7);
        file.md5_hash = res->isNull(8) ? std::string() : res->getString(8);
        file.sha1_hash = res->isNull(9) ? std::string() : res->getString(9);
        file.status = static_cast<int32_t>(res->getInt32(10));
        file.created_at = res->getTime(11);
        file.deleted_at = res->isNull(12) ? 0 : res->getTime(12);
        out.push_back(std::move(file));
    }
    return true;
}

bool StorageFileDAO::SoftDelete(uint64_t id, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }

    const char* sql = "UPDATE storage_files SET status = 2, deleted_at = NOW() WHERE id = ?";
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

bool StorageFileDAO::HardDelete(uint64_t id, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }

    const char* sql = "DELETE FROM storage_files WHERE id = ?";
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

bool StorageFileDAO::CleanupDeleted(std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }

    const char* sql =
        "DELETE FROM storage_files WHERE status = 2 AND deleted_at < DATE_SUB(NOW(), INTERVAL 30 "
        "DAY)";
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