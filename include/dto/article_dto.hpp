#pragma once
#include <string>
#include <vector>
#include "base/macro.hpp"

namespace IM::dto {

struct ArticleTagItem {
    uint64_t id = 0;
    std::string tag_name;
};

struct ArticleAnnexItem {
    uint64_t id = 0;
    uint64_t article_id = 0;
    std::string annex_name;
    int64_t annex_size = 0;
    std::string annex_path;
    std::string created_at;
    std::string deleted_at; // For recover list
};

struct ArticleClassifyItem {
    uint64_t id = 0;
    std::string class_name;
    int count = 0; // 文章数量
    int is_default = 0;
    int sort = 0;
};

struct ArticleItem {
    uint64_t id = 0;
    std::string title;
    std::string abstract;
    std::string image;
    uint64_t classify_id = 0;
    std::string classify_name;
    int is_asterisk = 0;
    int status = 0;
    std::string created_at;
    std::string updated_at;
    std::vector<ArticleTagItem> tags;
};

struct ArticleDetail {
    uint64_t id = 0;
    std::string title;
    std::string abstract;
    std::string image;
    std::string md_content;
    uint64_t classify_id = 0;
    std::string classify_name;
    int is_asterisk = 0;
    int status = 0;
    std::string created_at;
    std::string updated_at;
    std::vector<ArticleTagItem> tags;
    std::vector<ArticleAnnexItem> annex_list;
};

}
