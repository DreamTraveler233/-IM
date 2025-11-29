#include "infra/repository/article_repository_impl.hpp"

#include <sstream>

#include "util/util.hpp"

namespace IM::infra::repository {

// Classify
bool ArticleRepositoryImpl::CreateClassify(IM::MySQL::ptr conn, model::ArticleClassify& classify,
                                           std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "INSERT INTO im_article_classify (user_id, class_name, is_default, sort, created_at, "
        "updated_at) VALUES (?, ?, ?, ?, NOW(), NOW())";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, classify.user_id);
    stmt->bindString(2, classify.class_name);
    stmt->bindInt32(3, classify.is_default);
    stmt->bindInt32(4, classify.sort);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    classify.id = stmt->getLastInsertId();
    return true;
}

bool ArticleRepositoryImpl::UpdateClassify(IM::MySQL::ptr conn,
                                           const model::ArticleClassify& classify,
                                           std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "UPDATE im_article_classify SET class_name=?, is_default=?, sort=?, updated_at=NOW() WHERE "
        "id=?";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindString(1, classify.class_name);
    stmt->bindInt32(2, classify.is_default);
    stmt->bindInt32(3, classify.sort);
    stmt->bindUint64(4, classify.id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool ArticleRepositoryImpl::DeleteClassify(IM::MySQL::ptr conn, uint64_t classify_id,
                                           std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql = "UPDATE im_article_classify SET deleted_at=NOW() WHERE id=?";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, classify_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool ArticleRepositoryImpl::GetClassifyList(IM::MySQL::ptr conn, uint64_t user_id,
                                            std::vector<dto::ArticleClassifyItem>& list,
                                            std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "SELECT c.id, c.class_name, c.is_default, c.sort, (SELECT COUNT(*) FROM im_article a WHERE "
        "a.classify_id = c.id AND a.deleted_at IS NULL) as count FROM im_article_classify c WHERE "
        "c.user_id = ? AND c.deleted_at IS NULL ORDER BY c.sort ASC";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, user_id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    while (res->next()) {
        dto::ArticleClassifyItem item;
        item.id = res->getInt64(0);
        item.class_name = res->getString(1);
        item.is_default = res->getInt32(2);
        item.sort = res->getInt32(3);
        item.count = res->getInt32(4);
        list.push_back(item);
    }
    return true;
}

bool ArticleRepositoryImpl::GetClassify(IM::MySQL::ptr conn, uint64_t classify_id,
                                        model::ArticleClassify& classify, std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "SELECT id, user_id, class_name, is_default, sort FROM im_article_classify WHERE id = ?";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, classify_id);
    auto res = stmt->query();
    if (!res || !res->next()) {
        if (err) *err = "classify not found";
        return false;
    }
    classify.id = res->getInt64(0);
    classify.user_id = res->getInt64(1);
    classify.class_name = res->getString(2);
    classify.is_default = res->getInt32(3);
    classify.sort = res->getInt32(4);
    return true;
}

bool ArticleRepositoryImpl::SortClassify(IM::MySQL::ptr conn, uint64_t user_id,
                                         uint64_t classify_id, int sort_index, std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql = "UPDATE im_article_classify SET sort=? WHERE id=? AND user_id=?";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindInt32(1, sort_index);
    stmt->bindUint64(2, classify_id);
    stmt->bindUint64(3, user_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

// Article
bool ArticleRepositoryImpl::CreateArticle(IM::MySQL::ptr conn, model::Article& article,
                                          std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "INSERT INTO im_article (user_id, classify_id, title, abstract, md_content, image, "
        "is_asterisk, status, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, NOW(), "
        "NOW())";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, article.user_id);
    if (article.classify_id == 0) {
        stmt->bindNull(2);
    } else {
        stmt->bindUint64(2, article.classify_id);
    }
    stmt->bindString(3, article.title);
    stmt->bindString(4, article.abstract);
    stmt->bindString(5, article.md_content);
    stmt->bindString(6, article.image);
    stmt->bindInt32(7, article.is_asterisk);
    stmt->bindInt32(8, article.status);

    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    article.id = stmt->getLastInsertId();
    return true;
}

bool ArticleRepositoryImpl::UpdateArticle(IM::MySQL::ptr conn, const model::Article& article,
                                          std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "UPDATE im_article SET title=?, abstract=?, md_content=?, image=?, classify_id=?, "
        "status=?, updated_at=NOW() WHERE id=?";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindString(1, article.title);
    stmt->bindString(2, article.abstract);
    stmt->bindString(3, article.md_content);
    stmt->bindString(4, article.image);
    if (article.classify_id == 0) {
        stmt->bindNull(5);
    } else {
        stmt->bindUint64(5, article.classify_id);
    }
    stmt->bindInt32(6, article.status);
    stmt->bindUint64(7, article.id);

    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool ArticleRepositoryImpl::DeleteArticle(IM::MySQL::ptr conn, uint64_t article_id, bool forever,
                                          std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    std::string sql;
    if (forever) {
        sql = "DELETE FROM im_article WHERE id=?";
    } else {
        sql = "UPDATE im_article SET deleted_at=NOW() WHERE id=?";
    }
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, article_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool ArticleRepositoryImpl::RecoverArticle(IM::MySQL::ptr conn, uint64_t article_id,
                                           std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql = "UPDATE im_article SET deleted_at=NULL WHERE id=?";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, article_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool ArticleRepositoryImpl::GetArticle(IM::MySQL::ptr conn, uint64_t article_id,
                                       model::Article& article, std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "SELECT id, user_id, classify_id, title, abstract, md_content, image, is_asterisk, status, "
        "created_at, updated_at FROM im_article WHERE id=?";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, article_id);
    auto res = stmt->query();
    if (!res || !res->next()) {
        if (err) *err = "article not found";
        return false;
    }
    article.id = res->getInt64(0);
    article.user_id = res->getInt64(1);
    if (res->isNull(2)) {
        article.classify_id = 0;
    } else {
        article.classify_id = res->getInt64(2);
    }
    article.title = res->getString(3);
    article.abstract = res->getString(4);
    article.md_content = res->getString(5);
    article.image = res->getString(6);
    article.is_asterisk = res->getInt32(7);
    article.status = res->getInt32(8);
    article.created_at = res->getString(9);
    article.updated_at = res->getString(10);
    return true;
}

bool ArticleRepositoryImpl::GetArticleList(IM::MySQL::ptr conn, uint64_t user_id, int page,
                                           int size, uint64_t classify_id,
                                           const std::string& keyword, int find_type,
                                           std::vector<dto::ArticleItem>& list, int& total,
                                           std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }

    std::string from_clause = " FROM im_article a";
    std::string join_clause = "";
    std::string where_clause = "";

    if (find_type == 1) {  // Asterisk
        join_clause += " JOIN im_article_asterisk s ON a.id = s.article_id";
        where_clause = " WHERE s.user_id = ? AND a.deleted_at IS NULL";
    } else {
        where_clause = " WHERE a.user_id = ?";
        if (find_type == 2) {  // Recycle
            where_clause += " AND a.deleted_at IS NOT NULL";
        } else {  // Normal
            where_clause += " AND a.deleted_at IS NULL";
            if (classify_id > 0) {
                where_clause += " AND a.classify_id = ?";
            }
        }
    }

    if (!keyword.empty()) {
        where_clause += " AND (a.title LIKE ? OR a.abstract LIKE ?)";
    }

    // Count
    std::string count_sql = "SELECT COUNT(*) as total" + from_clause + join_clause + where_clause;
    auto count_stmt = conn->prepare(count_sql);
    if (!count_stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }

    int idx = 1;
    count_stmt->bindUint64(idx++, user_id);
    if (find_type != 1 && find_type != 2 && classify_id > 0) {
        count_stmt->bindUint64(idx++, classify_id);
    }
    if (!keyword.empty()) {
        std::string kw = "%" + keyword + "%";
        count_stmt->bindString(idx++, kw);
        count_stmt->bindString(idx++, kw);
    }

    auto count_res = count_stmt->query();
    if (count_res && count_res->next()) {
        total = count_res->getInt32(0);
    } else {
        total = 0;
    }

    if (total == 0) return true;

    // List
    std::string list_sql =
        "SELECT a.id, a.title, a.abstract, a.image, a.classify_id, c.class_name, a.is_asterisk, "
        "a.status, a.created_at, a.updated_at" +
        from_clause + " LEFT JOIN im_article_classify c ON a.classify_id = c.id" + join_clause +
        where_clause + " ORDER BY a.created_at DESC LIMIT ? OFFSET ?";

    auto list_stmt = conn->prepare(list_sql);
    if (!list_stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }

    idx = 1;
    list_stmt->bindUint64(idx++, user_id);
    if (find_type != 1 && find_type != 2 && classify_id > 0) {
        list_stmt->bindUint64(idx++, classify_id);
    }
    if (!keyword.empty()) {
        std::string kw = "%" + keyword + "%";
        list_stmt->bindString(idx++, kw);
        list_stmt->bindString(idx++, kw);
    }
    list_stmt->bindInt32(idx++, size);
    list_stmt->bindInt32(idx++, (page - 1) * size);

    auto res = list_stmt->query();
    if (!res) {
        if (err) *err = "query article list failed";
        return false;
    }

    while (res->next()) {
        dto::ArticleItem item;
        item.id = res->getInt64(0);
        item.title = res->getString(1);
        item.abstract = res->getString(2);
        item.image = res->getString(3);
        if (res->isNull(4)) {
            item.classify_id = 0;
        } else {
            item.classify_id = res->getInt64(4);
        }
        if (res->isNull(5)) {
            item.classify_name = "";
        } else {
            item.classify_name = res->getString(5);
        }
        item.is_asterisk = res->getInt32(6);
        item.status = res->getInt32(7);
        item.created_at = res->getString(8);
        item.updated_at = res->getString(9);
        list.push_back(item);
    }
    return true;
}

// Tags
bool ArticleRepositoryImpl::UpdateArticleTags(IM::MySQL::ptr conn, uint64_t article_id,
                                              const std::vector<std::string>& tags,
                                              std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    // 1. Clear existing map
    const char* del_sql = "DELETE FROM im_article_tag_map WHERE article_id=?";
    auto del_stmt = conn->prepare(del_sql);
    if (!del_stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    del_stmt->bindUint64(1, article_id);
    if (del_stmt->execute() != 0) {
        if (err) *err = del_stmt->getErrStr();
        return false;
    }

    if (tags.empty()) return true;

    // 2. Get user_id from article
    model::Article article;
    if (!GetArticle(conn, article_id, article, err)) return false;

    for (const auto& tag_name : tags) {
        // Ensure tag exists
        const char* tag_sql =
            "INSERT IGNORE INTO im_article_tag (user_id, tag_name, created_at, updated_at) VALUES "
            "(?, ?, NOW(), NOW())";
        auto tag_stmt = conn->prepare(tag_sql);
        if (tag_stmt) {
            tag_stmt->bindUint64(1, article.user_id);
            tag_stmt->bindString(2, tag_name);
            tag_stmt->execute();
        }

        // Get tag id
        const char* get_tag_sql = "SELECT id FROM im_article_tag WHERE user_id=? AND tag_name=?";
        auto get_tag_stmt = conn->prepare(get_tag_sql);
        if (get_tag_stmt) {
            get_tag_stmt->bindUint64(1, article.user_id);
            get_tag_stmt->bindString(2, tag_name);
            auto res = get_tag_stmt->query();
            if (res && res->next()) {
                uint64_t tag_id = res->getInt64(0);
                // Link
                const char* link_sql =
                    "INSERT INTO im_article_tag_map (article_id, tag_id) VALUES (?, ?)";
                auto link_stmt = conn->prepare(link_sql);
                if (link_stmt) {
                    link_stmt->bindUint64(1, article_id);
                    link_stmt->bindUint64(2, tag_id);
                    link_stmt->execute();
                }
            }
        }
    }
    return true;
}

bool ArticleRepositoryImpl::GetArticleTags(IM::MySQL::ptr conn, uint64_t article_id,
                                           std::vector<dto::ArticleTagItem>& tags,
                                           std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "SELECT t.id, t.tag_name FROM im_article_tag t JOIN im_article_tag_map m ON t.id = "
        "m.tag_id WHERE m.article_id=?";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, article_id);
    auto res = stmt->query();
    if (!res) return false;

    while (res->next()) {
        dto::ArticleTagItem item;
        item.id = res->getInt64(0);
        item.tag_name = res->getString(1);
        tags.push_back(item);
    }
    return true;
}

// Asterisk
bool ArticleRepositoryImpl::SetArticleAsterisk(IM::MySQL::ptr conn, uint64_t user_id,
                                               uint64_t article_id, bool is_asterisk,
                                               std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    // Update redundant field
    const char* up_sql = "UPDATE im_article SET is_asterisk=? WHERE id=?";
    auto up_stmt = conn->prepare(up_sql);
    if (up_stmt) {
        up_stmt->bindInt32(1, is_asterisk ? 1 : 2);
        up_stmt->bindUint64(2, article_id);
        up_stmt->execute();
    }

    // Update relation table
    if (is_asterisk) {
        const char* sql =
            "INSERT IGNORE INTO im_article_asterisk (article_id, user_id, created_at) VALUES (?, "
            "?, NOW())";
        auto stmt = conn->prepare(sql);
        if (!stmt) {
            if (err) *err = conn->getErrStr();
            return false;
        }
        stmt->bindUint64(1, article_id);
        stmt->bindUint64(2, user_id);
        return stmt->execute() == 0;
    } else {
        const char* sql = "DELETE FROM im_article_asterisk WHERE article_id=? AND user_id=?";
        auto stmt = conn->prepare(sql);
        if (!stmt) {
            if (err) *err = conn->getErrStr();
            return false;
        }
        stmt->bindUint64(1, article_id);
        stmt->bindUint64(2, user_id);
        return stmt->execute() == 0;
    }
}

// Annex
bool ArticleRepositoryImpl::AddAnnex(IM::MySQL::ptr conn, model::ArticleAnnex& annex,
                                     std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "INSERT INTO im_article_annex (article_id, user_id, annex_name, annex_size, annex_path, "
        "mime_type, created_at) VALUES (?, ?, ?, ?, ?, ?, NOW())";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, annex.article_id);
    stmt->bindUint64(2, annex.user_id);
    stmt->bindString(3, annex.annex_name);
    stmt->bindUint64(4, annex.annex_size);
    stmt->bindString(5, annex.annex_path);
    stmt->bindString(6, annex.mime_type);

    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    annex.id = stmt->getLastInsertId();
    return true;
}

bool ArticleRepositoryImpl::DeleteAnnex(IM::MySQL::ptr conn, uint64_t annex_id, bool forever,
                                        std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    std::string sql;
    if (forever) {
        sql = "DELETE FROM im_article_annex WHERE id=?";
    } else {
        sql = "UPDATE im_article_annex SET deleted_at=NOW() WHERE id=?";
    }
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, annex_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool ArticleRepositoryImpl::RecoverAnnex(IM::MySQL::ptr conn, uint64_t annex_id, std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql = "UPDATE im_article_annex SET deleted_at=NULL WHERE id=?";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, annex_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool ArticleRepositoryImpl::GetAnnexList(IM::MySQL::ptr conn, uint64_t article_id,
                                         std::vector<dto::ArticleAnnexItem>& list,
                                         std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "SELECT id, article_id, annex_name, annex_size, annex_path, created_at FROM "
        "im_article_annex WHERE article_id=? AND deleted_at IS NULL";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, article_id);
    auto res = stmt->query();
    if (!res) return false;

    while (res->next()) {
        dto::ArticleAnnexItem item;
        item.id = res->getInt64(0);
        item.article_id = res->getInt64(1);
        item.annex_name = res->getString(2);
        item.annex_size = res->getInt64(3);
        item.annex_path = res->getString(4);
        item.created_at = res->getString(5);
        list.push_back(item);
    }
    return true;
}

bool ArticleRepositoryImpl::GetRecycleAnnexList(IM::MySQL::ptr conn, uint64_t user_id,
                                                std::vector<dto::ArticleAnnexItem>& list,
                                                std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "SELECT id, article_id, annex_name, annex_size, annex_path, created_at, deleted_at FROM "
        "im_article_annex WHERE user_id=? AND deleted_at IS NOT NULL ORDER BY deleted_at DESC";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, user_id);
    auto res = stmt->query();
    if (!res) return false;

    while (res->next()) {
        dto::ArticleAnnexItem item;
        item.id = res->getInt64(0);
        item.article_id = res->getInt64(1);
        item.annex_name = res->getString(2);
        item.annex_size = res->getInt64(3);
        item.annex_path = res->getString(4);
        item.created_at = res->getString(5);
        item.deleted_at = res->getString(6);
        list.push_back(item);
    }
    return true;
}

bool ArticleRepositoryImpl::GetAnnex(IM::MySQL::ptr conn, uint64_t annex_id,
                                     model::ArticleAnnex& annex, std::string* err) {
    if (!conn) {
        if (err) *err = "connection is null";
        return false;
    }
    const char* sql =
        "SELECT id, article_id, user_id, annex_name, annex_path FROM im_article_annex WHERE id=?";
    auto stmt = conn->prepare(sql);
    if (!stmt) {
        if (err) *err = conn->getErrStr();
        return false;
    }
    stmt->bindUint64(1, annex_id);
    auto res = stmt->query();
    if (!res || !res->next()) return false;

    annex.id = res->getInt64(0);
    annex.article_id = res->getInt64(1);
    annex.user_id = res->getInt64(2);
    annex.annex_name = res->getString(3);
    annex.annex_path = res->getString(4);
    return true;
}

}  // namespace IM::infra::repository
