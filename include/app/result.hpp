#ifndef __CIM_APP_RESULT_HPP__
#define __CIM_APP_RESULT_HPP__

#include "dao/contact_apply_dao.hpp"
#include "dao/contact_dao.hpp"
#include "dao/contact_group_dao.hpp"
#include "dao/message_dao.hpp"
#include "dao/sms_code_dao.hpp"
#include "dao/talk_session_dao.hpp"
#include "dao/user_dao.hpp"
#include "dao/user_login_log_dao.hpp"
#include "dao/user_settings_dao.hpp"
#include "util/id_worker.hpp"

namespace CIM::app {

template <typename T>
struct Result {
    bool ok = false;
    int code = 0;     // 可选：错误码
    std::string err;  // 错误描述
    T data;           // 成功时的载荷
};

using VoidResult = Result<int>;  // data 不用时可用占位
using StatusResult = Result<std::string>;
using ApplyCountResult = Result<uint64_t>;
using UserResult = Result<CIM::dao::User>;
using SmsCodeResult = Result<CIM::dao::SmsCode>;
using UserInfoResult = Result<CIM::dao::UserInfo>;
using ConfigInfoResult = Result<CIM::dao::ConfigInfo>;
using ContactGroupResult = Result<CIM::dao::ContactGroup>;
using TalkSessionResult = Result<CIM::dao::TalkSessionItem>;
using ContactDetailsResult = Result<CIM::dao::ContactDetails>;
using MessageRecordPageResult = Result<CIM::dao::MessagePage>;
using MessageRecordResult = Result<CIM::dao::MessageRecord>; // 单条消息结果（发送返回）
using ContactListResult = Result<std::vector<CIM::dao::ContactItem>>;
using MessageRecordListResult = Result<std::vector<CIM::dao::MessageRecord>>;
using TalkSessionListResult = Result<std::vector<CIM::dao::TalkSessionItem>>;
using ContactApplyListResult = Result<std::vector<CIM::dao::ContactApplyItem>>;
using ContactGroupListResult = Result<std::vector<CIM::dao::ContactGroupItem>>;

}  // namespace CIM::app

#endif  // __CIM_APP_RESULT_HPP__