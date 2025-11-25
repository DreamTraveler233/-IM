#pragma once
#include <memory>
#include <vector>
#include <string>
#include "base/macro.hpp"
#include "common/common.hpp"
#include "dto/article_dto.hpp"

namespace IM::domain::service {

class IArticleService {
public:
    using Ptr = std::shared_ptr<IArticleService>;
    virtual ~IArticleService() = default;

    // Classify
    // 获取分类列表
    virtual Result<std::vector<dto::ArticleClassifyItem>> GetClassifyList(uint64_t user_id) = 0;
    // 编辑/创建分类 (id=0 create)
    virtual Result<void> EditClassify(uint64_t user_id, uint64_t classify_id, const std::string& name) = 0;
    // 删除分类
    virtual Result<void> DeleteClassify(uint64_t user_id, uint64_t classify_id) = 0;
    // 分类排序
    virtual Result<void> SortClassify(uint64_t user_id, uint64_t classify_id, int sort_index) = 0;

    // Article
    // 编辑/创建文章
    virtual Result<uint64_t> EditArticle(uint64_t user_id, uint64_t article_id, const std::string& title, const std::string& abstract, const std::string& content, const std::string& image, uint64_t classify_id, int status) = 0;
    // 删除文章 (移入回收站)
    virtual Result<void> DeleteArticle(uint64_t user_id, uint64_t article_id) = 0;
    // 永久删除文章
    virtual Result<void> ForeverDeleteArticle(uint64_t user_id, uint64_t article_id) = 0;
    // 恢复文章
    virtual Result<void> RecoverArticle(uint64_t user_id, uint64_t article_id) = 0;
    // 获取文章详情
    virtual Result<dto::ArticleDetail> GetArticleDetail(uint64_t user_id, uint64_t article_id) = 0;
    // 获取文章列表 (find_type: 0=普通, 1=星标, 2=回收站)
    virtual Result<std::pair<std::vector<dto::ArticleItem>, int>> GetArticleList(uint64_t user_id, int page, int size, uint64_t classify_id, const std::string& keyword, int find_type) = 0;
    // 移动文章分类
    virtual Result<void> MoveArticle(uint64_t user_id, uint64_t article_id, uint64_t classify_id) = 0;
    // 设置文章标签
    virtual Result<void> SetArticleTags(uint64_t user_id, uint64_t article_id, const std::vector<std::string>& tags) = 0;
    // 设置星标 (type: 1=star, 2=unstar)
    virtual Result<void> SetArticleAsterisk(uint64_t user_id, uint64_t article_id, int type) = 0;

    // Annex
    // 上传附件记录
    virtual Result<void> UploadAnnex(uint64_t user_id, uint64_t article_id, const std::string& name, int64_t size, const std::string& path, const std::string& mime) = 0;
    // 删除附件 (移入回收站)
    virtual Result<void> DeleteAnnex(uint64_t user_id, uint64_t annex_id) = 0;
    // 永久删除附件
    virtual Result<void> ForeverDeleteAnnex(uint64_t user_id, uint64_t annex_id) = 0;
    // 恢复附件
    virtual Result<void> RecoverAnnex(uint64_t user_id, uint64_t annex_id) = 0;
    // 获取回收站附件列表
    virtual Result<std::vector<dto::ArticleAnnexItem>> GetRecycleAnnexList(uint64_t user_id) = 0;
};

}
