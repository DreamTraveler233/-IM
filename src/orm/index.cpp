#include "orm/index.hpp"

#include "base/macro.hpp"
#include "util/util.hpp"

namespace CIM::orm {
static Logger::ptr g_logger = CIM_LOG_NAME("orm");

Index::Type Index::ParseType(const std::string& v) {
#define XX(a, b)  \
    if (v == b) { \
        return a; \
    }
    XX(TYPE_PK, "pk");
    XX(TYPE_UNIQ, "uniq");
    XX(TYPE_INDEX, "index");
#undef XX
    return TYPE_NULL;
}

std::string Index::TypeToString(Type v) {
#define XX(a, b)  \
    if (v == a) { \
        return b; \
    }
    XX(TYPE_PK, "pk");
    XX(TYPE_UNIQ, "uniq");
    XX(TYPE_INDEX, "index");
#undef XX
    return "";
}

bool Index::init(const tinyxml2::XMLElement& node) {
    if (!node.Attribute("name")) {
        CIM_LOG_ERROR(g_logger) << "index name not exists";
        return false;
    }
    m_name = node.Attribute("name");

    if (!node.Attribute("type")) {
        CIM_LOG_ERROR(g_logger) << "index name=" << m_name << " type is null";
        return false;
    }

    m_type = node.Attribute("type");
    m_dtype = ParseType(m_type);
    if (m_dtype == TYPE_NULL) {
        CIM_LOG_ERROR(g_logger) << "index name=" << m_name << " type=" << m_type
                                << " invalid (pk, index, uniq)";
        return false;
    }

    if (!node.Attribute("cols")) {
        CIM_LOG_ERROR(g_logger) << "index name=" << m_name << " cols is null";
    }
    std::string tmp = node.Attribute("cols");
    m_cols = split(tmp, ',');

    if (node.Attribute("desc")) {
        m_desc = node.Attribute("desc");
    }
    return true;
}

}  // namespace CIM::orm