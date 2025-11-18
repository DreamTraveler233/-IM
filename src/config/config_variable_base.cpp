#include "config/config_variable_base.hpp"

#include <algorithm>

#include "base/macro.hpp"

namespace CIM {
ConfigVariableBase::ConfigVariableBase(const std::string& name, const std::string& description)
    : m_name(name), m_description(description) {
    CIM_ASSERT(!name.empty());
    std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::tolower);
}

const std::string& ConfigVariableBase::getName() const {
    return m_name;
}

const std::string& ConfigVariableBase::getDescription() const {
    return m_description;
}
}  // namespace CIM