#pragma once
#include <memory>
#include <vector>
#include <string>
#include "db/mysql.hpp"
#include "model/article.hpp"
#include "dto/article_dto.hpp"

namespace IM::domain::repository {

class IArticleRepository {
public:
    using Ptr = std::shared_ptr<IArticleRepository>;
    virtual ~IArticleRepository() = default;

    // Classify
    // 创建分类
    virtual bool CreateClassify(IM::MySQL::ptr conn, model::ArticleClassify& classify, std::string* err) = 0;
    // 更新分类
    virtual bool UpdateClassify(IM::MySQL::ptr conn, const model::ArticleClassify& classify, std::string* err) = 0;
    // 删除分类
    virtual bool DeleteClassify(IM::MySQL::ptr conn, uint64_t classify_id, std::string* err) = 0;
    // 获取分类列表
    virtual bool GetClassifyList(IM::MySQL::ptr conn, uint64_t user_id, std::vector<dto::ArticleClassifyItem>& list, std::string* err) = 0;
    // 获取单个分类
    virtual bool GetClassify(IM::MySQL::ptr conn, uint64_t classify_id, model::ArticleClassify& classify, std::string* err) = 0;
    // 排序分类
    virtual bool SortClassify(IM::MySQL::ptr conn, uint64_t user_id, uint64_t classify_id, int sort_index, std::string* err) = 0;

    // Article
    // 创建文章
    virtual bool CreateArticle(IM::MySQL::ptr conn, model::Article& article, std::string* err) = 0;
    // 更新文章
    virtual bool UpdateArticle(IM::MySQL::ptr conn, const model::Article& article, std::string* err) = 0;
    // 删除文章 (forever=true 物理删除, false 软删除)
    virtual bool DeleteArticle(IM::MySQL::ptr conn, uint64_t article_id, bool forever, std::string* err) = 0;
    // 恢复文章
    virtual bool RecoverArticle(IM::MySQL::ptr conn, uint64_t article_id, std::string* err) = 0;
    // 获取文章详情
    virtual bool GetArticle(IM::MySQL::ptr conn, uint64_t article_id, model::Article& article, std::string* err) = 0;
    
    // 获取文章列表
    // find_type: 0=普通列表(含搜索/分类), 1=收藏列表, 2=回收站列表
    virtual bool GetArticleList(IM::MySQL::ptr conn, uint64_t user_id, int page, int size, 
                                uint64_t classify_id, const std::string& keyword, int find_type, 
                                std::vector<dto::ArticleItem>& list, int& total, std::string* err) = 0;

    // Tags
    // 更新文章标签 (全量替换)
    virtual bool UpdateArticleTags(IM::MySQL::ptr conn, uint64_t article_id, const std::vector<std::string>& tags, std::string* err) = 0;
    // 获取文章标签
    virtual bool GetArticleTags(IM::MySQL::ptr conn, uint64_t article_id, std::vector<dto::ArticleTagItem>& tags, std::string* err) = 0;

    // Asterisk
    // 设置/取消星标
    virtual bool SetArticleAsterisk(IM::MySQL::ptr conn, uint64_t user_id, uint64_t article_id, bool is_asterisk, std::string* err) = 0;

    // Annex
    // 添加附件
    virtual bool AddAnnex(IM::MySQL::ptr conn, model::ArticleAnnex& annex, std::string* err) = 0;
    // 删除附件
    virtual bool DeleteAnnex(IM::MySQL::ptr conn, uint64_t annex_id, bool forever, std::string* err) = 0;
    // 恢复附件
    virtual bool RecoverAnnex(IM::MySQL::ptr conn, uint64_t annex_id, std::string* err) = 0;
    // 获取文章附件列表
    virtual bool GetAnnexList(IM::MySQL::ptr conn, uint64_t article_id, std::vector<dto::ArticleAnnexItem>& list, std::string* err) = 0;
    // 获取回收站附件列表
    virtual bool GetRecycleAnnexList(IM::MySQL::ptr conn, uint64_t user_id, std::vector<dto::ArticleAnnexItem>& list, std::string* err) = 0;
    // 获取单个附件
    virtual bool GetAnnex(IM::MySQL::ptr conn, uint64_t annex_id, model::ArticleAnnex& annex, std::string* err) = 0;
};

}
