#include "api/article_api_module.hpp"

#include "app/article_service_impl.hpp"
#include "infra/repository/article_repository_impl.hpp"
#include "base/macro.hpp"
#include "common/common.hpp"
#include "http/http_server.hpp"
#include "http/http_servlet.hpp"
#include "system/application.hpp"
#include "util/util.hpp"

namespace IM::api {
static auto g_logger = IM_LOG_NAME("root");

ArticleApiModule::ArticleApiModule() : Module("api.article", "0.1.0", "builtin") {
    auto repo = std::make_shared<IM::infra::repository::ArticleRepositoryImpl>();
    m_article_service = std::make_shared<IM::app::ArticleServiceImpl>(repo);
}

bool ArticleApiModule::onServerReady() {
    std::vector<IM::TcpServer::ptr> httpServers;
    if (!IM::Application::GetInstance()->getServer("http", httpServers)) {
        IM_LOG_WARN(g_logger) << "no http servers found when registering article routes";
        return true;
    }

    for (auto& s : httpServers) {
        auto http = std::dynamic_pointer_cast<IM::http::HttpServer>(s);
        if (!http) continue;
        auto dispatch = http->getServletDispatch();

        // 文章附件
        dispatch->addServlet("/api/v1/article-annex/delete",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t annex_id = IM::JsonUtil::GetUint64(body, "annex_id");
                                 auto result = m_article_service->DeleteAnnex(uid_result.data, annex_id);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/article-annex/forever-delete",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t annex_id = IM::JsonUtil::GetUint64(body, "annex_id");
                                 auto result = m_article_service->ForeverDeleteAnnex(uid_result.data, annex_id);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/article-annex/recover",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t annex_id = IM::JsonUtil::GetUint64(body, "annex_id");
                                 auto result = m_article_service->RecoverAnnex(uid_result.data, annex_id);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/article-annex/recover-list",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 auto result = m_article_service->GetRecycleAnnexList(uid_result.data);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 Json::Value d;
                                 Json::Value list(Json::arrayValue);
                                 for (const auto& item : result.data) {
                                     Json::Value v;
                                     v["id"] = (Json::UInt64)item.id;
                                     v["annex_name"] = item.annex_name;
                                     v["annex_size"] = (Json::UInt64)item.annex_size;
                                     v["created_at"] = item.created_at;
                                     v["deleted_at"] = item.deleted_at;
                                     list.append(v);
                                 }
                                 d["list"] = list;
                                 res->setBody(Ok(d));
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/article-annex/upload",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 res->setBody(Ok());
                                 return 0;
                             });

        // 文章分类
        dispatch->addServlet("/api/v1/article/classify/delete",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t classify_id = IM::JsonUtil::GetUint64(body, "classify_id");
                                 auto result = m_article_service->DeleteClassify(uid_result.data, classify_id);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/article/classify/edit",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t classify_id = IM::JsonUtil::GetUint64(body, "classify_id");
                                 std::string class_name = IM::JsonUtil::GetString(body, "class_name");
                                 auto result = m_article_service->EditClassify(uid_result.data, classify_id, class_name);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 Json::Value d(Json::objectValue);
                                 d["id"] = (Json::UInt64)classify_id; 
                                 res->setBody(Ok(d));
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/article/classify/list",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 auto result = m_article_service->GetClassifyList(uid_result.data);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 Json::Value d;
                                 Json::Value list(Json::arrayValue);
                                 for (const auto& item : result.data) {
                                     Json::Value v;
                                     v["id"] = (Json::UInt64)item.id;
                                     v["class_name"] = item.class_name;
                                     v["count"] = item.count;
                                     v["is_default"] = item.is_default;
                                     v["sort"] = item.sort;
                                     list.append(v);
                                 }
                                 d["list"] = list;
                                 res->setBody(Ok(d));
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/article/classify/sort",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t classify_id = IM::JsonUtil::GetUint64(body, "classify_id");
                                 int sort_index = IM::JsonUtil::GetInt32(body, "sort_index");
                                 auto result = m_article_service->SortClassify(uid_result.data, classify_id, sort_index);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });

        // 文章核心接口
        dispatch->addServlet("/api/v1/article/delete",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t article_id = IM::JsonUtil::GetUint64(body, "article_id");
                                 auto result = m_article_service->DeleteArticle(uid_result.data, article_id);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/article/detail",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t article_id = IM::JsonUtil::GetUint64(body, "article_id");
                                 auto result = m_article_service->GetArticleDetail(uid_result.data, article_id);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 Json::Value d(Json::objectValue);
                                 d["id"] = (Json::UInt64)result.data.id;
                                 d["title"] = result.data.title;
                                 d["abstract"] = result.data.abstract;
                                 d["image"] = result.data.image;
                                 d["md_content"] = result.data.md_content;
                                 d["classify_id"] = (Json::UInt64)result.data.classify_id;
                                 d["classify_name"] = result.data.classify_name;
                                 d["is_asterisk"] = result.data.is_asterisk;
                                 d["status"] = result.data.status;
                                 d["created_at"] = result.data.created_at;
                                 d["updated_at"] = result.data.updated_at;
                                 
                                 Json::Value tags(Json::arrayValue);
                                 for(const auto& t : result.data.tags) {
                                     Json::Value tv;
                                     tv["id"] = (Json::UInt64)t.id;
                                     tv["tag_name"] = t.tag_name;
                                     tags.append(tv);
                                 }
                                 d["tags"] = tags;

                                 Json::Value annex(Json::arrayValue);
                                 for(const auto& a : result.data.annex_list) {
                                     Json::Value av;
                                     av["id"] = (Json::UInt64)a.id;
                                     av["annex_name"] = a.annex_name;
                                     av["annex_size"] = (Json::UInt64)a.annex_size;
                                     av["annex_path"] = a.annex_path;
                                     av["created_at"] = a.created_at;
                                     annex.append(av);
                                 }
                                 d["annex_list"] = annex;

                                 res->setBody(Ok(d));
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/article/editor",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t article_id = IM::JsonUtil::GetUint64(body, "article_id");
                                 std::string title = IM::JsonUtil::GetString(body, "title");
                                 std::string abstract = IM::JsonUtil::GetString(body, "abstract");
                                 std::string md_content = IM::JsonUtil::GetString(body, "md_content");
                                 std::string image = IM::JsonUtil::GetString(body, "image");
                                 uint64_t classify_id = IM::JsonUtil::GetUint64(body, "classify_id");
                                 int status = IM::JsonUtil::GetInt32(body, "status");

                                 auto result = m_article_service->EditArticle(uid_result.data, article_id, title, abstract, md_content, image, classify_id, status);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 Json::Value d;
                                 d["article_id"] = (Json::UInt64)result.data;
                                 res->setBody(Ok(d));
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/article/forever-delete",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t article_id = IM::JsonUtil::GetUint64(body, "article_id");
                                 auto result = m_article_service->ForeverDeleteArticle(uid_result.data, article_id);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/article/list",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 int page = IM::JsonUtil::GetInt32(body, "page");
                                 int size = IM::JsonUtil::GetInt32(body, "size");
                                 if (size == 0) size = 20;
                                 std::string keyword = IM::JsonUtil::GetString(body, "keyword");
                                 uint64_t classify_id = IM::JsonUtil::GetUint64(body, "classify_id");
                                 int find_type = IM::JsonUtil::GetInt32(body, "find_type");

                                 auto result = m_article_service->GetArticleList(uid_result.data, page, size, classify_id, keyword, find_type);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 Json::Value d;
                                 Json::Value list(Json::arrayValue);
                                 for (const auto& item : result.data.first) {
                                     Json::Value v;
                                     v["id"] = (Json::UInt64)item.id;
                                     v["title"] = item.title;
                                     v["abstract"] = item.abstract;
                                     v["image"] = item.image;
                                     v["classify_id"] = (Json::UInt64)item.classify_id;
                                     v["classify_name"] = item.classify_name;
                                     v["is_asterisk"] = item.is_asterisk;
                                     v["status"] = item.status;
                                     v["created_at"] = item.created_at;
                                     v["updated_at"] = item.updated_at;
                                     
                                     Json::Value tags(Json::arrayValue);
                                     for(const auto& t : item.tags) {
                                         Json::Value tv;
                                         tv["id"] = (Json::UInt64)t.id;
                                         tv["tag_name"] = t.tag_name;
                                         tags.append(tv);
                                     }
                                     v["tags"] = tags;
                                     list.append(v);
                                 }
                                 d["list"] = list;
                                 d["total"] = result.data.second;
                                 res->setBody(Ok(d));
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/article/move",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t article_id = IM::JsonUtil::GetUint64(body, "article_id");
                                 uint64_t classify_id = IM::JsonUtil::GetUint64(body, "classify_id");
                                 auto result = m_article_service->MoveArticle(uid_result.data, article_id, classify_id);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/article/recover",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t article_id = IM::JsonUtil::GetUint64(body, "article_id");
                                 auto result = m_article_service->RecoverArticle(uid_result.data, article_id);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/article/recover-list",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 int page = IM::JsonUtil::GetInt32(body, "page");
                                 int size = IM::JsonUtil::GetInt32(body, "size");
                                 if (size == 0) size = 20;
                                 
                                 auto result = m_article_service->GetArticleList(uid_result.data, page, size, 0, "", 2);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 Json::Value d;
                                 Json::Value list(Json::arrayValue);
                                 for (const auto& item : result.data.first) {
                                     Json::Value v;
                                     v["id"] = (Json::UInt64)item.id;
                                     v["title"] = item.title;
                                     v["abstract"] = item.abstract;
                                     v["image"] = item.image;
                                     v["classify_id"] = (Json::UInt64)item.classify_id;
                                     v["classify_name"] = item.classify_name;
                                     v["is_asterisk"] = item.is_asterisk;
                                     v["status"] = item.status;
                                     v["created_at"] = item.created_at;
                                     v["updated_at"] = item.updated_at;
                                     list.append(v);
                                 }
                                 d["list"] = list;
                                 d["total"] = result.data.second;
                                 res->setBody(Ok(d));
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/article/tags",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t article_id = IM::JsonUtil::GetUint64(body, "article_id");
                                 std::vector<std::string> tags;
                                 if (body.isMember("tags") && body["tags"].isArray()) {
                                     for (const auto& t : body["tags"]) {
                                         tags.push_back(t.asString());
                                     }
                                 }
                                 auto result = m_article_service->SetArticleTags(uid_result.data, article_id, tags);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });
        dispatch->addServlet("/api/v1/article/asterisk",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }
                                 Json::Value body;
                                 if (!ParseBody(req->getBody(), body)) {
                                     res->setStatus(IM::http::HttpStatus::BAD_REQUEST);
                                     return 0;
                                 }
                                 uint64_t article_id = IM::JsonUtil::GetUint64(body, "article_id");
                                 int type = IM::JsonUtil::GetInt32(body, "type");
                                 auto result = m_article_service->SetArticleAsterisk(uid_result.data, article_id, type);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }
                                 res->setBody(Ok());
                                 return 0;
                             });
    }
    return true;
}

}  // namespace IM::api
