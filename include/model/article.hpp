#pragma once
#include <string>
#include <vector>
#include "base/macro.hpp"

namespace IM::model {

// 文章主表 im_article
struct Article {
    uint64_t id = 0;
    uint64_t user_id = 0;
    uint64_t classify_id = 0;
    std::string title;
    std::string abstract;
    std::string md_content;
    std::string image;
    int is_asterisk = 2; // 1=收藏 2=未收藏 (冗余)
    int status = 1;      // 1=正常 2=草稿
    std::string deleted_at;
    std::string created_at;
    std::string updated_at;
};

// 文章分类 im_article_classify
struct ArticleClassify {
    uint64_t id = 0;
    uint64_t user_id = 0;
    std::string class_name;
    int is_default = 2; // 1=是 2=否
    int sort = 0;
    std::string created_at;
    std::string updated_at;
    std::string deleted_at;
};

// 文章标签 im_article_tag
struct ArticleTag {
    uint64_t id = 0;
    uint64_t user_id = 0;
    std::string tag_name;
    std::string created_at;
    std::string updated_at;
};

// 文章附件 im_article_annex
struct ArticleAnnex {
    uint64_t id = 0;
    uint64_t article_id = 0;
    uint64_t user_id = 0;
    std::string annex_name;
    int64_t annex_size = 0;
    std::string annex_path;
    std::string mime_type;
    std::string deleted_at;
    std::string created_at;
};

// 文章收藏 im_article_asterisk
struct ArticleAsterisk {
    uint64_t article_id = 0;
    uint64_t user_id = 0;
    std::string created_at;
};

}
