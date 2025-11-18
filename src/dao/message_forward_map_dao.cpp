#include "dao/message_forward_map_dao.hpp"

namespace CIM::dao {

static constexpr const char* kDBName = "default";

bool MessageForwardMapDao::AddForwardMap(const std::shared_ptr<CIM::MySQL>& db,
                                         const std::string& forward_msg_id,
                                         const std::vector<ForwardSrc>& sources, std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    if (sources.empty()) return true;
    const char* sql =
        "INSERT INTO im_message_forward_map "
        "(forward_msg_id,src_msg_id,src_talk_id,src_sender_id,created_at) VALUES (?,?,?,?,NOW())";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    for (auto& s : sources) {
        stmt->bindString(1, forward_msg_id);
        stmt->bindString(2, s.src_msg_id);
        stmt->bindUint64(3, s.src_talk_id);
        stmt->bindUint64(4, s.src_sender_id);
        if (stmt->execute() != 0) {
            if (err) *err = stmt->getErrStr();
            return false;
        }
    }
    return true;
}
}  // namespace CIM::dao
