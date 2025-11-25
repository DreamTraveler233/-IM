#ifndef __IM_API_ARTICLE_API_MODULE_HPP__
#define __IM_API_ARTICLE_API_MODULE_HPP__

#include "other/module.hpp"
#include "domain/service/article_service.hpp"

namespace IM::api {

class ArticleApiModule : public IM::Module {
   public:
    ArticleApiModule();
    ~ArticleApiModule() override = default;

    bool onServerReady() override;

   private:
    IM::domain::service::IArticleService::Ptr m_article_service;
};

}  // namespace IM::api

#endif // __IM_API_ARTICLE_API_MODULE_HPP__