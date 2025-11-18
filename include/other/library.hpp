#ifndef __CIM_LIBRARY_HPP__
#define __CIM_LIBRARY_HPP__

#include <memory>

#include "module.hpp"

namespace CIM {
class Library {
   public:
    static Module::ptr GetModule(const std::string& path);
};

}  // namespace CIM

#endif