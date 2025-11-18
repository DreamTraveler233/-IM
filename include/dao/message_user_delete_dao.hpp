#ifndef __CIM_DAO_MESSAGE_USER_DELETE_DAO_HPP__
#define __CIM_DAO_MESSAGE_USER_DELETE_DAO_HPP__

#include <cstdint>
#include <string>

namespace CIM::dao {
// im_message_user_delete 表 DAO：记录用户侧删除消息（仅影响本人视图）
class MessageUserDeleteDao {
   public:
    static bool MarkUserDelete(const std::string& msg_id, const uint64_t user_id, std::string* err = nullptr);
};
}  // namespace CIM::dao

#endif  // __CIM_DAO_MESSAGE_USER_DELETE_DAO_HPP__
