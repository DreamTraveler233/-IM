#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace CIM::dao {

struct StorageFile {
    uint64_t id = 0;             // 文件ID
    uint64_t user_id = 0;        // 上传用户ID，可为空（匿名上传）
    int32_t drive = 1;           // 存储驱动：1本地 2OSS等
    std::string file_name;       // 原始文件名
    std::string file_ext;        // 文件扩展名
    std::string mime_type;       // MIME类型
    uint64_t file_size = 0;      // 文件大小（字节）
    std::string file_path;       // 文件存储路径
    std::string md5_hash;        // MD5哈希，用于去重
    std::string sha1_hash;       // SHA1哈希，备用
    int32_t status = 1;          // 文件状态：1正常 2删除
    std::time_t created_at = 0;  // 创建时间
    std::time_t deleted_at = 0;  // 删除时间（软删除）
};

class StorageFileDAO {
   public:
    // 创建文件记录
    static bool Create(const StorageFile& file, uint64_t& out_id, std::string* err = nullptr);

    // 根据ID获取文件信息
    static bool GetById(uint64_t id, StorageFile& out);

    // 根据MD5哈希查找文件（去重）
    static bool GetByMd5(const std::string& md5_hash, StorageFile& out);

    // 根据用户ID获取文件列表
    static bool GetByUserId(uint64_t user_id, std::vector<StorageFile>& out, int limit = 100);

    // 软删除文件
    static bool SoftDelete(uint64_t id, std::string* err = nullptr);

    // 永久删除文件
    static bool HardDelete(uint64_t id, std::string* err = nullptr);

    // 清理已删除的文件记录
    static bool CleanupDeleted(std::string* err = nullptr);
};

}  // namespace CIM::dao