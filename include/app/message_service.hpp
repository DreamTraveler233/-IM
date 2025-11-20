#ifndef __CIM_APP_MESSAGE_SERVICE_HPP__
#define __CIM_APP_MESSAGE_SERVICE_HPP__

#include <cstdint>
#include <string>
#include <vector>

#include "app/result.hpp"

namespace CIM::app {

// 消息服务：提供消息列表/历史/转发/删除/撤回等业务操作。
// 说明：
// - 仅封装 DAO 与简单权限校验；复杂业务（敏感词、风控、群成员校验）应由更上层处理。
// - 分页逻辑：cursor=0 表示获取最新；返回的 cursor=当前页最小 sequence，用于下一次继续向旧消息翻页。
class MessageService {
   public:
    // 获取当前会话消息（按 sequence DESC 返回最新 -> 较旧）。
    static MessageRecordPageResult LoadRecords(const uint64_t current_user_id,
                                               const uint8_t talk_mode, const uint64_t to_from_id,
                                               uint64_t cursor, uint32_t limit);

    // 获取当前会话历史消息（支持按 msg_type 过滤，0=全部）。
    static MessageRecordPageResult LoadHistoryRecords(const uint64_t current_user_id,
                                                      const uint8_t talk_mode,
                                                      const uint64_t to_from_id,
                                                      const uint16_t msg_type, uint64_t cursor,
                                                      uint32_t limit);

    // 获取转发消息记录（传入一组消息ID，返回消息详情；不分页）。
    static MessageRecordListResult LoadForwardRecords(const uint64_t current_user_id,
                                                      const uint8_t talk_mode,
                                                      const std::vector<std::string>& msg_ids);

    // 删除聊天记录（仅对本人视图生效）。
    static VoidResult DeleteMessages(const uint64_t current_user_id, const uint8_t talk_mode,
                                     const uint64_t to_from_id,
                                     const std::vector<std::string>& msg_ids);

   // 删除会话中该用户的所有消息（仅影响该用户视图）并删除会话视图
   static VoidResult DeleteAllMessagesInTalkForUser(const uint64_t current_user_id,
                                       const uint8_t talk_mode,
                                       const uint64_t to_from_id);

    // 清空聊天记录（软删除，将消息插入删除表中）
    static VoidResult ClearTalkRecords(const uint64_t current_user_id, const uint8_t talk_mode,
                                       const uint64_t to_from_id);

    // 撤回消息（仅发送者可撤回，后续可扩展管理员权限）。
    static VoidResult RevokeMessage(const uint64_t current_user_id, const uint8_t talk_mode,
                                    const uint64_t to_from_id, const std::string& msg_id);
    // 发送消息：返回刚插入的消息记录（含补充字段）。
    // 参数：
    // - current_user_id: 发送者
    // - talk_mode: 1单聊 2群聊
    // - to_from_id: 单聊对端用户ID / 群聊群ID
    // - msg_type: 消息类型（与前端枚举一致）
    // - content_text: 文本类消息正文（非文本留空）
    // - extra: 非文本/扩展字段 JSON 字符串（文本可为空）
    // - quote_msg_id: 引用消息ID（可选 0 表示无）
    static MessageRecordResult SendMessage(const uint64_t current_user_id, const uint8_t talk_mode,
                                           const uint64_t to_from_id, const uint16_t msg_type,
                                           const std::string& content_text,
                                           const std::string& extra,
                                           const std::string& quote_msg_id,
                                           const std::string& msg_id,
                                           const std::vector<uint64_t>& mentioned_user_ids);

   // 更新消息状态（通常由发送方请求，或服务器端发生发送失败时标记）
   static VoidResult UpdateMessageStatus(const uint64_t current_user_id, const uint8_t talk_mode,
                                const uint64_t to_from_id, const std::string& msg_id,
                                uint8_t status);

   private:
    // 根据 talk_mode 与对象ID 获取会话 talk_id（不存在返回 0）。
    static uint64_t resolveTalkId(const uint8_t talk_mode, const uint64_t to_from_id);

    // 将 DAO Message 转换为前端需要的记录结构（补充用户昵称头像、引用）。
    static bool buildRecord(const CIM::dao::Message& msg, CIM::dao::MessageRecord& out,
                            std::string* err);
};

}  // namespace CIM::app

#endif  // __CIM_APP_MESSAGE_SERVICE_HPP__