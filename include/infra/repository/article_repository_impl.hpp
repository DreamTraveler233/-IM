#pragma once
#include "domain/repository/article_repository.hpp"

namespace IM::infra::repository {

class ArticleRepositoryImpl : public domain::repository::IArticleRepository {
public:
    // Classify
    bool CreateClassify(IM::MySQL::ptr conn, model::ArticleClassify& classify, std::string* err) override;
    bool UpdateClassify(IM::MySQL::ptr conn, const model::ArticleClassify& classify, std::string* err) override;
    bool DeleteClassify(IM::MySQL::ptr conn, uint64_t classify_id, std::string* err) override;
    bool GetClassifyList(IM::MySQL::ptr conn, uint64_t user_id, std::vector<dto::ArticleClassifyItem>& list, std::string* err) override;
    bool GetClassify(IM::MySQL::ptr conn, uint64_t classify_id, model::ArticleClassify& classify, std::string* err) override;
    bool SortClassify(IM::MySQL::ptr conn, uint64_t user_id, uint64_t classify_id, int sort_index, std::string* err) override;

    // Article
    bool CreateArticle(IM::MySQL::ptr conn, model::Article& article, std::string* err) override;
    bool UpdateArticle(IM::MySQL::ptr conn, const model::Article& article, std::string* err) override;
    bool DeleteArticle(IM::MySQL::ptr conn, uint64_t article_id, bool forever, std::string* err) override;
    bool RecoverArticle(IM::MySQL::ptr conn, uint64_t article_id, std::string* err) override;
    bool GetArticle(IM::MySQL::ptr conn, uint64_t article_id, model::Article& article, std::string* err) override;
    bool GetArticleList(IM::MySQL::ptr conn, uint64_t user_id, int page, int size, 
                        uint64_t classify_id, const std::string& keyword, int find_type, 
                        std::vector<dto::ArticleItem>& list, int& total, std::string* err) override;

    // Tags
    bool UpdateArticleTags(IM::MySQL::ptr conn, uint64_t article_id, const std::vector<std::string>& tags, std::string* err) override;
    bool GetArticleTags(IM::MySQL::ptr conn, uint64_t article_id, std::vector<dto::ArticleTagItem>& tags, std::string* err) override;

    // Asterisk
    bool SetArticleAsterisk(IM::MySQL::ptr conn, uint64_t user_id, uint64_t article_id, bool is_asterisk, std::string* err) override;

    // Annex
    bool AddAnnex(IM::MySQL::ptr conn, model::ArticleAnnex& annex, std::string* err) override;
    bool DeleteAnnex(IM::MySQL::ptr conn, uint64_t annex_id, bool forever, std::string* err) override;
    bool RecoverAnnex(IM::MySQL::ptr conn, uint64_t annex_id, std::string* err) override;
    bool GetAnnexList(IM::MySQL::ptr conn, uint64_t article_id, std::vector<dto::ArticleAnnexItem>& list, std::string* err) override;
    bool GetRecycleAnnexList(IM::MySQL::ptr conn, uint64_t user_id, std::vector<dto::ArticleAnnexItem>& list, std::string* err) override;
    bool GetAnnex(IM::MySQL::ptr conn, uint64_t annex_id, model::ArticleAnnex& annex, std::string* err) override;
};

}
