#ifndef __CIM_DAO_MESSAGE_FORWARD_MAP_DAO_HPP__
#define __CIM_DAO_MESSAGE_FORWARD_MAP_DAO_HPP__

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "db/mysql.hpp"

namespace CIM::dao {
// 转发来源映射字段
struct ForwardSrc {
    std::string src_msg_id;      // 原消息ID（CHAR(32)）
    uint64_t src_talk_id = 0;    // 原消息会话ID
    uint64_t src_sender_id = 0;  // 原发送者
};

// im_message_forward_map 表 DAO：转发消息与来源消息的对应关系
class MessageForwardMapDao {
   public:
    static bool AddForwardMap(const std::shared_ptr<CIM::MySQL>& db, const std::string& forward_msg_id,
                              const std::vector<ForwardSrc>& sources, std::string* err = nullptr);
};
}  // namespace CIM::dao

#endif  // __CIM_DAO_MESSAGE_FORWARD_MAP_DAO_HPP__
