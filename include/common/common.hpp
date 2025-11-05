#ifndef __CIM_COMMON_COMMON_HPP__
#define __CIM_COMMON_COMMON_HPP__

#include <jsoncpp/json/json.h>

#include <string>

#include "http.hpp"

namespace CIM {

/* 构造成功响应的JSON字符串 */
std::string Ok(const Json::Value& data = Json::Value(Json::objectValue));

/* 构造错误响应的JSON字符串 */
std::string Error(int code, const std::string& msg);

/* 解析请求体中的JSON字符串 */
bool ParseBody(const std::string& body, Json::Value& out);

// JWT 签发：返回签名后的 JWT 字符串
std::string SignJwt(const std::string& uid, uint32_t expires_in);

// JWT 校验：有效则返回 true 并输出 uid（字符串）
bool VerifyJwt(const std::string& token, std::string* out_uid = nullptr);

// JWT 是否过期
bool IsJwtExpired(const std::string& token);

// 从请求中提取 uid
uint64_t GetUidFromToken(CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res);

}  // namespace CIM

#endif  // __CIM_COMMON_COMMON_HPP__