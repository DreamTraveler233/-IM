#ifndef __IM_API_GROUP_API_MODULE_HPP__
#define __IM_API_GROUP_API_MODULE_HPP__

#include "other/module.hpp"
#include "domain/service/group_service.hpp"

namespace IM::api {

class GroupApiModule : public IM::Module {
   public:
    GroupApiModule(IM::domain::service::IGroupService::Ptr group_service);
    ~GroupApiModule() override = default;

    bool onServerReady() override;

   private:
    IM::domain::service::IGroupService::Ptr m_group_service;
};

}  // namespace IM::api

#endif // __IM_API_GROUP_API_MODULE_HPP__