#ifndef __APP_SETTING_SERVICE_HPP__
#define __APP_SETTING_SERVICE_HPP__

#include <cstdint>
#include <optional>
#include <string>

#include "dao/user_dao.hpp"

namespace CIM::app {

struct SettingResult {
    bool ok = false;      // 是否成功
    std::string err;      // 错误描述
    CIM::dao::User user;  // 成功时的用户信息
};

class SettingService {
   public:
    // 加载用户信息
    static SettingResult LoadUserSettings(uint64_t uid);
};

}  // namespace CIM::app

#endif  // __SETTING_SERVICE_HPP__
