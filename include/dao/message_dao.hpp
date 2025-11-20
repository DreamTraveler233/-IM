#ifndef __CIM_DAO_MESSAGE_DAO_HPP__
#define __CIM_DAO_MESSAGE_DAO_HPP__

#include <cstdint>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include "db/mysql.hpp"

namespace CIM::dao {

// 消息主表实体（im_message）
struct Message {
    std::string id;               // 消息ID（CHAR(32) UUID/HEX）
    uint64_t talk_id = 0;         // 会话ID
    uint64_t sequence = 0;        // 会话内序号
    uint8_t talk_mode = 0;        // 1=单聊 2=群聊
    uint16_t msg_type = 0;        // 消息类型
    uint64_t sender_id = 0;       // 发送者
    uint64_t receiver_id = 0;     // 单聊对端，群聊为空(0表示NULL)
    uint64_t group_id = 0;        // 群聊群ID，单聊为空(0表示NULL)
    std::string content_text;     // 文本/系统文本内容
    std::string extra;            // JSON 扩展（各类型专属字段）
    std::string quote_msg_id;     // 被引用消息ID（空字符串表示NULL）
    uint8_t is_revoked = 2;       // 1=已撤回 2=正常
    uint64_t revoke_by = 0;       // 撤回人(0表示NULL)
    std::time_t revoke_time = 0;  // 撤回时间(0表示NULL)
    uint8_t status = 1;           // 发送状态: 1=成功 2=发送中 3=失败
    std::time_t created_at = 0;   // 创建时间
    std::time_t updated_at = 0;   // 更新时间
};

struct MessageRecord {
    std::string msg_id;      // 消息ID字符串（原始为整数）
    uint64_t sequence = 0;   // 会话内序号
    uint16_t msg_type = 0;   // 消息类型
    uint64_t from_id = 0;    // 发送者用户ID
    std::string nickname;    // 发送者昵称
    std::string avatar;      // 发送者头像
    uint8_t is_revoked = 2;  // 撤回状态
    uint8_t status = 1;      // 发送状态：1成功 2发送中 3失败
    std::string send_time;   // 发送时间字符串
    std::string extra;       // 额外 JSON
    std::string quote;       // 引用消息 JSON
};

struct MessagePage {
    std::vector<MessageRecord> items;
    uint64_t cursor = 0;  // 下一次拉取的锚点（旧消息最小 sequence）
};

class MessageDao {
   public:
    // 创建消息（写入 im_message）；不包含转发/已读/提及等附表逻辑。
    // 说明：sequence 需由 TalkSequenceDao 保证递增并在外层事务中调用本方法。
    static bool Create(const std::shared_ptr<CIM::MySQL>& db, const Message& m,
                       std::string* err = nullptr);

    // 根据消息ID查询。
    static bool GetById(const std::string& msg_id, Message& out, std::string* err = nullptr);

    // 会话内按序号倒序分页（anchor_seq=0 时取最新）。返回 sequence 递减排序结果。
    static bool ListRecentDesc(const uint64_t talk_id, const uint64_t anchor_seq,
                               const size_t limit, std::vector<Message>& out,
                               std::string* err = nullptr);

    // 与 ListRecentDesc 功能类似，但可下推过滤条件：
    // - user_id != 0 时，排除该用户在 im_message_user_delete 表中标记为删除的消息
    // - msg_type != 0 时，仅返回指定 msg_type 的消息
    static bool ListRecentDescWithFilter(const uint64_t talk_id, const uint64_t anchor_seq,
                                         const size_t limit, const uint64_t user_id,
                                         const uint16_t msg_type, std::vector<Message>& out,
                                         std::string* err = nullptr);

    // 与 ListRecentDesc 功能类似
    static bool ListRecentDescWithFilter(const std::shared_ptr<CIM::MySQL>& db,
                                         const uint64_t talk_id, const uint64_t anchor_seq,
                                         const size_t limit, const uint64_t user_id,
                                         const uint16_t msg_type, std::vector<Message>& out,
                                         std::string* err = nullptr);

    // 会话内获取大于某序号的消息（升序）。
    static bool ListAfterAsc(const uint64_t talk_id, const uint64_t after_seq, const size_t limit,
                             std::vector<Message>& out, std::string* err = nullptr);

    // 批量根据 ids 获取消息（用于批量加载被引用的消息，避免 N+1 查询）
    static bool GetByIds(const std::vector<std::string>& ids, std::vector<Message>& out,
                         std::string* err = nullptr);

    // 批量根据 ids 获取消息，并可排除某个用户已删除的消息
    static bool GetByIdsWithFilter(const std::vector<std::string>& ids, const uint64_t user_id,
                                   std::vector<Message>& out, std::string* err = nullptr);

    // 撤回消息（状态置 1），仅当当前状态为正常(2)。
    static bool Revoke(const std::shared_ptr<CIM::MySQL>& db, const std::string& msg_id,
                       const uint64_t user_id, std::string* err = nullptr);

    // 硬删除会话下的所有消息
    static bool DeleteByTalkId(const std::shared_ptr<CIM::MySQL>& db, const uint64_t talk_id,
                               std::string* err = nullptr);
    // 更新消息的发送状态（用于标记失败/成功等）
    static bool SetStatus(const std::shared_ptr<CIM::MySQL>& db, const std::string& msg_id,
                          uint8_t status, std::string* err = nullptr);
};

}  // namespace CIM::dao

#endif  // __CIM_DAO_MESSAGE_DAO_HPP__
