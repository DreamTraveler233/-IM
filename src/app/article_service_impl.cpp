#include "app/article_service_impl.hpp"

#include "db/mysql.hpp"

namespace IM::app {

static constexpr const char* kDBName = "default";

ArticleServiceImpl::ArticleServiceImpl(domain::repository::IArticleRepository::Ptr repo)
    : repo_(repo) {}

// Classify
Result<std::vector<dto::ArticleClassifyItem>> ArticleServiceImpl::GetClassifyList(
    uint64_t user_id) {
    auto conn = IM::MySQLMgr::GetInstance()->get(kDBName);
    std::vector<dto::ArticleClassifyItem> list;
    std::string err;
    if (!repo_->GetClassifyList(conn, user_id, list, &err)) {
        return Result<std::vector<dto::ArticleClassifyItem>>::Error(500, err);
    }
    return Result<std::vector<dto::ArticleClassifyItem>>::Success(list);
}

Result<void> ArticleServiceImpl::EditClassify(uint64_t user_id, uint64_t classify_id,
                                              const std::string& name) {
    auto conn = IM::MySQLMgr::GetInstance()->get(kDBName);
    std::string err;
    if (classify_id == 0) {
        model::ArticleClassify classify;
        classify.user_id = user_id;
        classify.class_name = name;
        if (!repo_->CreateClassify(conn, classify, &err)) {
            return Result<void>::Error(500, err);
        }
    } else {
        model::ArticleClassify classify;
        if (!repo_->GetClassify(conn, classify_id, classify, &err)) {
            return Result<void>::Error(500, err);
        }
        if (classify.user_id != user_id) return Result<void>::Error(403, "permission denied");
        classify.class_name = name;
        if (!repo_->UpdateClassify(conn, classify, &err)) {
            return Result<void>::Error(500, err);
        }
    }
    return Result<void>::Success();
}

Result<void> ArticleServiceImpl::DeleteClassify(uint64_t user_id, uint64_t classify_id) {
    auto conn = IM::MySQLMgr::GetInstance()->get(kDBName);
    std::string err;
    model::ArticleClassify classify;
    if (!repo_->GetClassify(conn, classify_id, classify, &err)) {
        return Result<void>::Error(500, err);
    }
    if (classify.user_id != user_id) return Result<void>::Error(403, "permission denied");
    if (classify.is_default == 1) return Result<void>::Error(400, "cannot delete default classify");

    if (!repo_->DeleteClassify(conn, classify_id, &err)) {
        return Result<void>::Error(500, err);
    }
    return Result<void>::Success();
}

Result<void> ArticleServiceImpl::SortClassify(uint64_t user_id, uint64_t classify_id,
                                              int sort_index) {
    auto conn = IM::MySQLMgr::GetInstance()->get(kDBName);
    std::string err;
    if (!repo_->SortClassify(conn, user_id, classify_id, sort_index, &err)) {
        return Result<void>::Error(500, err);
    }
    return Result<void>::Success();
}

// Article
Result<uint64_t> ArticleServiceImpl::EditArticle(
    uint64_t user_id, uint64_t article_id, const std::string& title, const std::string& abstract,
    const std::string& content, const std::string& image, uint64_t classify_id, int status) {
    auto conn = IM::MySQLMgr::GetInstance()->get(kDBName);
    std::string err;
    if (article_id == 0) {
        model::Article article;
        article.user_id = user_id;
        article.title = title;
        article.abstract = abstract;
        article.md_content = content;
        article.image = image;
        article.classify_id = classify_id;
        article.status = status;
        if (!repo_->CreateArticle(conn, article, &err)) {
            return Result<uint64_t>::Error(500, err);
        }
        return Result<uint64_t>::Success(article.id);
    } else {
        model::Article article;
        if (!repo_->GetArticle(conn, article_id, article, &err)) {
            return Result<uint64_t>::Error(500, err);
        }
        if (article.user_id != user_id) return Result<uint64_t>::Error(403, "permission denied");

        article.title = title;
        article.abstract = abstract;
        article.md_content = content;
        article.image = image;
        article.classify_id = classify_id;
        article.status = status;
        if (!repo_->UpdateArticle(conn, article, &err)) {
            return Result<uint64_t>::Error(500, err);
        }
        return Result<uint64_t>::Success(article.id);
    }
}

Result<void> ArticleServiceImpl::DeleteArticle(uint64_t user_id, uint64_t article_id) {
    auto conn = IM::MySQLMgr::GetInstance()->get(kDBName);
    std::string err;
    model::Article article;
    if (!repo_->GetArticle(conn, article_id, article, &err)) {
        return Result<void>::Error(500, err);
    }
    if (article.user_id != user_id) return Result<void>::Error(403, "permission denied");

    if (!repo_->DeleteArticle(conn, article_id, false, &err)) {
        return Result<void>::Error(500, err);
    }
    return Result<void>::Success();
}

Result<void> ArticleServiceImpl::ForeverDeleteArticle(uint64_t user_id, uint64_t article_id) {
    auto conn = IM::MySQLMgr::GetInstance()->get(kDBName);
    std::string err;
    model::Article article;
    if (!repo_->GetArticle(conn, article_id, article, &err)) {
        return Result<void>::Error(500, err);
    }
    if (article.user_id != user_id) return Result<void>::Error(403, "permission denied");

    if (!repo_->DeleteArticle(conn, article_id, true, &err)) {
        return Result<void>::Error(500, err);
    }
    return Result<void>::Success();
}

Result<void> ArticleServiceImpl::RecoverArticle(uint64_t user_id, uint64_t article_id) {
    auto conn = IM::MySQLMgr::GetInstance()->get(kDBName);
    std::string err;
    model::Article article;
    if (!repo_->GetArticle(conn, article_id, article, &err)) {
        return Result<void>::Error(500, err);
    }
    if (article.user_id != user_id) return Result<void>::Error(403, "permission denied");

    if (!repo_->RecoverArticle(conn, article_id, &err)) {
        return Result<void>::Error(500, err);
    }
    return Result<void>::Success();
}

Result<dto::ArticleDetail> ArticleServiceImpl::GetArticleDetail(uint64_t user_id,
                                                                uint64_t article_id) {
    auto conn = IM::MySQLMgr::GetInstance()->get(kDBName);
    std::string err;
    model::Article article;
    if (!repo_->GetArticle(conn, article_id, article, &err)) {
        return Result<dto::ArticleDetail>::Error(500, err);
    }
    // Check permission? Usually public or own. Assuming own for now as per API context (editor).
    // If public, need to check if published. But let's assume user_id check for editing.
    if (article.user_id != user_id) {
        // If status is published, maybe allow? But this is "editor" API usually.
        // Let's stick to ownership check for safety.
        return Result<dto::ArticleDetail>::Error(403, "permission denied");
    }

    dto::ArticleDetail detail;
    detail.id = article.id;
    detail.title = article.title;
    detail.abstract = article.abstract;
    detail.image = article.image;
    detail.md_content = article.md_content;
    detail.classify_id = article.classify_id;
    detail.is_asterisk = article.is_asterisk;
    detail.status = article.status;
    detail.created_at = article.created_at;
    detail.updated_at = article.updated_at;

    // Classify Name
    if (article.classify_id > 0) {
        model::ArticleClassify classify;
        if (repo_->GetClassify(conn, article.classify_id, classify, nullptr)) {
            detail.classify_name = classify.class_name;
        }
    }

    // Tags
    repo_->GetArticleTags(conn, article_id, detail.tags, nullptr);

    // Annex
    repo_->GetAnnexList(conn, article_id, detail.annex_list, nullptr);

    return Result<dto::ArticleDetail>::Success(detail);
}

Result<std::pair<std::vector<dto::ArticleItem>, int>> ArticleServiceImpl::GetArticleList(
    uint64_t user_id, int page, int size, uint64_t classify_id, const std::string& keyword,
    int find_type) {
    auto conn = IM::MySQLMgr::GetInstance()->get(kDBName);
    std::string err;
    std::vector<dto::ArticleItem> list;
    int total = 0;
    if (!repo_->GetArticleList(conn, user_id, page, size, classify_id, keyword, find_type, list,
                               total, &err)) {
        return Result<std::pair<std::vector<dto::ArticleItem>, int>>::Error(500, err);
    }

    // Fill tags for each article
    for (auto& item : list) {
        repo_->GetArticleTags(conn, item.id, item.tags, nullptr);
    }

    return Result<std::pair<std::vector<dto::ArticleItem>, int>>::Success({list, total});
}

Result<void> ArticleServiceImpl::MoveArticle(uint64_t user_id, uint64_t article_id,
                                             uint64_t classify_id) {
    auto conn = IM::MySQLMgr::GetInstance()->get(kDBName);
    std::string err;
    model::Article article;
    if (!repo_->GetArticle(conn, article_id, article, &err)) {
        return Result<void>::Error(500, err);
    }
    if (article.user_id != user_id) return Result<void>::Error(403, "permission denied");

    article.classify_id = classify_id;
    if (!repo_->UpdateArticle(conn, article, &err)) {
        return Result<void>::Error(500, err);
    }
    return Result<void>::Success();
}

Result<void> ArticleServiceImpl::SetArticleTags(uint64_t user_id, uint64_t article_id,
                                                const std::vector<std::string>& tags) {
    auto conn = IM::MySQLMgr::GetInstance()->get(kDBName);
    std::string err;
    model::Article article;
    if (!repo_->GetArticle(conn, article_id, article, &err)) {
        return Result<void>::Error(500, err);
    }
    if (article.user_id != user_id) return Result<void>::Error(403, "permission denied");

    if (!repo_->UpdateArticleTags(conn, article_id, tags, &err)) {
        return Result<void>::Error(500, err);
    }
    return Result<void>::Success();
}

Result<void> ArticleServiceImpl::SetArticleAsterisk(uint64_t user_id, uint64_t article_id,
                                                    int type) {
    auto conn = IM::MySQLMgr::GetInstance()->get(kDBName);
    std::string err;
    // Verify ownership? Or can I star others' articles?
    // Usually I can star others. But here let's assume I can star any visible article.
    // But wait, the table `im_article_asterisk` links user and article.
    // The `im_article` table has `is_asterisk` which is redundant. If I star someone else's article, I shouldn't modify their `im_article` record's `is_asterisk` field if that field represents "Author's favorite" or something.
    // However, the SQL comment says "Redundant, real collection see table im_article_asterisk".
    // If `is_asterisk` in `im_article` is just a cache for the *current user* (which is impossible if multiple users view it), or it means "Author starred it".
    // Given the context of a personal blog/notes system (implied by "Article"), it's likely single-user or personal space.
    // If it's a multi-user system where I can star others, the `is_asterisk` on `im_article` is confusing.
    // Let's assume it's a personal knowledge base for now, or `is_asterisk` is updated only if `user_id` matches author.

    // For now, I'll just call repo.
    bool is_star = (type == 1);
    if (!repo_->SetArticleAsterisk(conn, user_id, article_id, is_star, &err)) {
        return Result<void>::Error(500, err);
    }
    return Result<void>::Success();
}

// Annex
Result<void> ArticleServiceImpl::UploadAnnex(uint64_t user_id, uint64_t article_id,
                                             const std::string& name, int64_t size,
                                             const std::string& path, const std::string& mime) {
    auto conn = IM::MySQLMgr::GetInstance()->get(kDBName);
    std::string err;
    model::ArticleAnnex annex;
    annex.user_id = user_id;
    annex.article_id = article_id;
    annex.annex_name = name;
    annex.annex_size = size;
    annex.annex_path = path;
    annex.mime_type = mime;

    if (!repo_->AddAnnex(conn, annex, &err)) {
        return Result<void>::Error(500, err);
    }
    return Result<void>::Success();
}

Result<void> ArticleServiceImpl::DeleteAnnex(uint64_t user_id, uint64_t annex_id) {
    auto conn = IM::MySQLMgr::GetInstance()->get(kDBName);
    std::string err;
    model::ArticleAnnex annex;
    if (!repo_->GetAnnex(conn, annex_id, annex, &err)) {
        return Result<void>::Error(500, err);
    }
    if (annex.user_id != user_id) return Result<void>::Error(403, "permission denied");

    if (!repo_->DeleteAnnex(conn, annex_id, false, &err)) {
        return Result<void>::Error(500, err);
    }
    return Result<void>::Success();
}

Result<void> ArticleServiceImpl::ForeverDeleteAnnex(uint64_t user_id, uint64_t annex_id) {
    auto conn = IM::MySQLMgr::GetInstance()->get(kDBName);
    std::string err;
    model::ArticleAnnex annex;
    if (!repo_->GetAnnex(conn, annex_id, annex, &err)) {
        return Result<void>::Error(500, err);
    }
    if (annex.user_id != user_id) return Result<void>::Error(403, "permission denied");

    if (!repo_->DeleteAnnex(conn, annex_id, true, &err)) {
        return Result<void>::Error(500, err);
    }
    return Result<void>::Success();
}

Result<void> ArticleServiceImpl::RecoverAnnex(uint64_t user_id, uint64_t annex_id) {
    auto conn = IM::MySQLMgr::GetInstance()->get(kDBName);
    std::string err;
    model::ArticleAnnex annex;
    if (!repo_->GetAnnex(conn, annex_id, annex, &err)) {
        return Result<void>::Error(500, err);
    }
    if (annex.user_id != user_id) return Result<void>::Error(403, "permission denied");

    if (!repo_->RecoverAnnex(conn, annex_id, &err)) {
        return Result<void>::Error(500, err);
    }
    return Result<void>::Success();
}

Result<std::vector<dto::ArticleAnnexItem>> ArticleServiceImpl::GetRecycleAnnexList(
    uint64_t user_id) {
    auto conn = IM::MySQLMgr::GetInstance()->get(kDBName);
    std::string err;
    std::vector<dto::ArticleAnnexItem> list;
    if (!repo_->GetRecycleAnnexList(conn, user_id, list, &err)) {
        return Result<std::vector<dto::ArticleAnnexItem>>::Error(500, err);
    }
    return Result<std::vector<dto::ArticleAnnexItem>>::Success(list);
}

}  // namespace IM::app
