#ifndef __CIM_DAO_TALK_DAO_HPP__
#define __CIM_DAO_TALK_DAO_HPP__

#include <cstdint>
#include <memory>
#include <string>

#include "db/mysql.hpp"

namespace CIM::dao {

// Talk 主实体（im_talk）的数据访问：保证单聊/群聊会话唯一性并提供查询。
// 说明：
// - 这里不做业务校验（如自聊、群成员关系），仅负责依赖唯一索引实现并发安全的 upsert/查询。
// - 推荐在事务内调用 findOrCreateXxx，以确保和下游对 session 的写入原子性。
class TalkDao {
   public:
    // 单聊：根据两个用户ID（自动排序为 user_min_id/user_max_id）查找或创建 talk。
    // 入参：db 为事务绑定连接；uid1/uid2 为两个用户ID（无需事先排序）。
    // 出参：talk_id 返回 im_talk.id。
    static bool findOrCreateSingleTalk(const std::shared_ptr<CIM::MySQL>& db, uint64_t uid1,
                                       uint64_t uid2, uint64_t& out_talk_id,
                                       std::string* err = nullptr);

    // 群聊：根据 group_id 查找或创建 talk。
    static bool findOrCreateGroupTalk(const std::shared_ptr<CIM::MySQL>& db, uint64_t group_id,
                                      uint64_t& out_talk_id, std::string* err = nullptr);

    // 仅查询：获取单聊 talk_id（不存在返回 false 且不写 err 或写入说明）。
   static bool getSingleTalkId(const uint64_t uid1, const uint64_t uid2, uint64_t& out_talk_id,
                        std::string* err = nullptr);

   // db 版本：在已有事务/连接中查询单聊 talk_id（不获取新的连接）。
   static bool getSingleTalkId(const std::shared_ptr<CIM::MySQL>& db, const uint64_t uid1,
                        const uint64_t uid2, uint64_t& out_talk_id,
                        std::string* err = nullptr);

    // 仅查询：获取群聊 talk_id。
   static bool getGroupTalkId(const uint64_t group_id, uint64_t& out_talk_id,
                        std::string* err = nullptr);

   // db 版本：在已有事务/连接中查询群聊 talk_id。
   static bool getGroupTalkId(const std::shared_ptr<CIM::MySQL>& db, const uint64_t group_id,
                        uint64_t& out_talk_id, std::string* err = nullptr);
};

}  // namespace CIM::dao

#endif  // __CIM_DAO_TALK_DAO_HPP__
