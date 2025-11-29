#ifndef __IM_API_WS_GATEWAY_MODULE_HPP__
#define __IM_API_WS_GATEWAY_MODULE_HPP__

#include "domain/service/user_service.hpp"
#include "domain/repository/talk_repository.hpp"
#include "other/module.hpp"

namespace IM::api {

class WsGatewayModule : public IM::Module {
   public:
    WsGatewayModule(IM::domain::service::IUserService::Ptr user_service,
                    IM::domain::repository::ITalkRepository::Ptr talk_repo);
    ~WsGatewayModule() override = default;

    bool onServerReady() override;

    // 主动推送通用事件到指定用户的所有在线连接
    static void PushToUser(uint64_t uid, const std::string& event,
                           const Json::Value& payload = Json::Value(),
                           const std::string& ackid = "");

    // 主动推送一条 IM 消息事件
    static void PushImMessage(uint8_t talk_mode, uint64_t to_from_id, uint64_t from_id,
                              const Json::Value& body);

   private:
    IM::domain::service::IUserService::Ptr m_user_service;
    IM::domain::repository::ITalkRepository::Ptr m_talk_repo;
};

}  // namespace IM::api

#endif  // __IM_API_WS_GATEWAY_MODULE_HPP__
