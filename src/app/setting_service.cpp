#include "app/setting_service.hpp"

#include "macro.hpp"


namespace CIM::app {
static auto g_logger = CIM_LOG_NAME("root");

SettingResult CIM::app::SettingService::LoadUserSettings(uint64_t uid) {
    SettingResult result;
    // 这里加载用户设置
    return result;
}

}  // namespace CIM::app