#ifndef __CIM_APP_RESULT_HPP__
#define __CIM_APP_RESULT_HPP__

#include <string>

namespace CIM::app {

template <typename T>
struct Result {
    bool ok = false;
    int code = 0;     // 可选：错误码
    std::string err;  // 错误描述
    T data;           // 成功时的载荷
};

using VoidResult = Result<int>;  // data 不用时可用占位
}  // namespace CIM::app

#endif  // __CIM_APP_RESULT_HPP__