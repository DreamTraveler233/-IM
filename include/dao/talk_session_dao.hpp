#ifndef __CIM_DAO_TALK_SESSION_HPP__
#define __CIM_DAO_TALK_SESSION_HPP__

#include <cstdint>
#include <ctime>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "db/mysql.hpp"

namespace CIM::dao {

struct TalkSessionItem {
    uint64_t id = 0;              // 会话ID
    uint8_t talk_mode = 1;        // 会话模式（1=单聊 2=群聊）
    uint64_t to_from_id = 0;      // 目标用户ID
    uint8_t is_top = 2;           // 是否置顶
    uint8_t is_disturb = 2;       // 是否免打扰
    uint8_t is_robot = 2;         // 是否机器人
    std::string name = "";        // 会话对象名称（用户名/群名称）
    std::string avatar = "";      // 会话对象头像URL
    std::string remark = "";      // 会话备注
    uint32_t unread_num = 0;      // 未读消息数
    std::string msg_text = "";    // 最后一条消息预览文本
    std::string updated_at = "";  // 最后更新时间
};

struct TalkSession {
    uint64_t user_id = 0;     // 用户ID
    uint64_t talk_id = 0;     // talk 主实体ID
    uint64_t to_from_id = 0;  // 目标用户ID/群ID
    uint8_t talk_mode = 1;    // 会话模式(1=单聊 2=群聊)

    uint8_t is_top = 2;      // 是否置顶(1=是 2=否)
    uint8_t is_disturb = 2;  // 是否免打扰(1=是 2=否)
    uint8_t is_robot = 2;    // 是否机器人(1=是 2=否)

    uint64_t last_ack_seq = 0;  // 最后已读序号
    uint32_t unread_num = 0;    // 未读消息数

    std::optional<std::string> last_msg_id;  // 最后一条消息的ID（CHAR(32)）
    // 上面字段类型改为字符串：

    std::optional<uint16_t> last_msg_type;       // 最后一条消息的类型
    std::optional<uint64_t> last_sender_id;      // 最后一条消息的发送者ID
    std::optional<std::string> last_msg_digest;  // 最后一条消息的摘要
    std::optional<std::string> draft_text;       // 草稿文本
    std::optional<std::string> name;             // 会话对象名称
    std::optional<std::string> avatar;           // 会话对象头像
    std::optional<std::string> remark;           // 会话备注
    std::optional<std::time_t> deleted_at;       // 软删除时间

    std::time_t created_at = 0;  // 创建时间
    std::time_t updated_at = 0;  // 更新时间
};

class TalkSessionDAO {
   public:
    // 获取用户的会话列表
    static bool getSessionListByUserId(const uint64_t user_id, std::vector<TalkSessionItem>& out,
                                       std::string* err = nullptr);
    // 设置会话置顶/取消置顶
    static bool setSessionTop(const uint64_t user_id, const uint64_t to_from_id,
                              const uint8_t talk_mode, const uint8_t action,
                              std::string* err = nullptr);
    // 设置会话免打扰/取消免打扰
    static bool setSessionDisturb(const uint64_t user_id, const uint64_t to_from_id,
                                  const uint8_t talk_mode, const uint8_t action,
                                  std::string* err = nullptr);
    // 创建会话
    static bool createSession(const std::shared_ptr<CIM::MySQL>& db, const TalkSession& session,
                              std::string* err = nullptr);
    // 获取会话信息
    static bool getSessionByUserId(const std::shared_ptr<CIM::MySQL>& db, const uint64_t user_id,
                                   TalkSessionItem& out, const uint64_t to_from_id,
                                   const uint8_t talk_mode, std::string* err = nullptr);
    // 删除会话
    static bool deleteSession(const uint64_t user_id, const uint64_t to_from_id,
                              const uint8_t talk_mode, std::string* err = nullptr);
    // 清除会话未读消息数
    static bool clearSessionUnreadNum(const uint64_t user_id, const uint64_t to_from_id,
                                      const uint8_t talk_mode, std::string* err = nullptr);

    // 新消息到达时，推进会话快照与未读数：
    // - 设置 last_msg_id/type/sender/digest/time，updated_at=NOW()
    // - 对除 sender 外的用户未读数 +1（软删除的会话不更新）
    static bool bumpOnNewMessage(const std::shared_ptr<CIM::MySQL>& db, const uint64_t talk_id,
                                 const uint64_t sender_user_id, const std::string& last_msg_id,
                                 const uint16_t last_msg_type, const std::string& last_msg_digest,
                                 std::string* err = nullptr);
    // 为指定用户更新会话的最后消息字段（用于用户删除消息后重建摘要）
    static bool updateLastMsgForUser(const uint64_t user_id, const uint64_t talk_id,
                                     const std::optional<std::string>& last_msg_id,
                                     const std::optional<uint16_t>& last_msg_type,
                                     const std::optional<uint64_t>& last_sender_id,
                                     const std::optional<std::string>& last_msg_digest,
                                     std::string* err = nullptr);
    // 列出对于指定 talk_id 且 last_msg_id 匹配的所有 user_id（用于撤回时重建会话摘要）
    static bool listUsersByLastMsg(const uint64_t talk_id, const std::string& last_msg_id,
                                   std::vector<uint64_t>& out_user_ids, std::string* err = nullptr);
    // 修改会话备注
    static bool EditRemarkWithConn(const std::shared_ptr<CIM::MySQL>& db, const uint64_t user_id,
                                   const uint64_t to_from_id, const std::string& remark,
                                   std::string* err = nullptr);
};
}  // namespace CIM::dao
#endif  // __CIM_DAO_TALK_SESSION_HPP__