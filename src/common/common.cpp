#include "common/common.hpp"

#include <jwt-cpp/jwt.h>

#include "base/macro.hpp"
#include "config/config.hpp"
#include "net/tcp_server.hpp"
#include "other/crypto_module.hpp"
#include "util/json_util.hpp"

namespace CIM {

static auto g_logger = CIM_LOG_NAME("system");

// JWT签名密钥
static auto g_jwt_secret = CIM::Config::Lookup<std::string>(
    "auth.jwt.secret", std::string("dev-secret"), "jwt hmac secret");
// JWT签发者
static auto g_jwt_issuer =
    CIM::Config::Lookup<std::string>("auth.jwt.issuer", std::string("auth-service"), "jwt issuer");

std::string Ok(const Json::Value& data) {
    return CIM::JsonUtil::ToString(data);
}

std::string Error(const int code, const std::string& msg) {
    Json::Value root;
    root["code"] = code;
    root["message"] = msg;
    root["data"] = Json::nullValue;
    return CIM::JsonUtil::ToString(root);
}

bool ParseBody(const std::string& body, Json::Value& out) {
    if (body.empty()) return false;
    if (!CIM::JsonUtil::FromString(out, body)) return false;
    return out.isObject();
}

TokenResult SignJwt(const std::string& uid, uint32_t expires_in) {
    TokenResult result;
    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::seconds(expires_in);
    try {
        result.data = jwt::create()
                          .set_type("JWS")
                          .set_issuer(g_jwt_issuer->getValue())
                          .set_issued_at(now)
                          .set_expires_at(exp)
                          .set_subject(uid)
                          .set_payload_claim("uid", jwt::claim(uid))
                          .sign(jwt::algorithm::hs256{g_jwt_secret->getValue()});
    } catch (const std::exception& e) {
        CIM_LOG_ERROR(g_logger) << result.err;
        result.code = 500;
        result.err = "令牌签名失败！";
        return result;
    }

    result.ok = true;
    return result;
}

/**
 * @brief 验证JWT令牌的有效性
 * @param token[in] 待验证的JWT令牌字符串
 * @param out_uid[out] 如果验证成功且该参数非空，则输出令牌中的用户ID
 * @return 验证成功返回true，否则返回false
 * 
 * 该函数使用HS256算法和预设的密钥来验证JWT令牌，
 * 同时检查签发者信息是否匹配。如果令牌有效且包含
 * uid声明，则将其写入out_uid参数中。
 */
bool VerifyJwt(const std::string& token, std::string* out_uid) {
    try {
        auto dec = jwt::decode(token);
        auto verifier = jwt::verify()
                            .allow_algorithm(jwt::algorithm::hs256{g_jwt_secret->getValue()})
                            .with_issuer(g_jwt_issuer->getValue());
        verifier.verify(dec);
        if (out_uid) {
            if (dec.has_payload_claim("uid")) {
                *out_uid = dec.get_payload_claim("uid").as_string();
            } else {
                *out_uid = "";
            }
        }
        return true;
    } catch (const std::exception& e) {
        CIM_LOG_WARN(g_logger) << "jwt verify failed: " << e.what();
        return false;
    }
}

bool IsJwtExpired(const std::string& token) {
    try {
        auto dec = jwt::decode(token);
        if (dec.has_expires_at()) {
            auto exp = dec.get_expires_at();
            return exp < std::chrono::system_clock::now();
        }
    } catch (const std::exception& e) {
        CIM_LOG_WARN(g_logger) << "jwt decode failed: " << e.what();
    }
    return false;
}

UidResult GetUidFromToken(CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res) {
    UidResult result;

    // 从请求头中提取 Token
    std::string header = req->getHeader("Authorization", "");
    std::string token;
    // 兼容 "Bearer <token>" 格式；严格校验前缀与长度，避免 substr 越界
    const std::string kBearer = "Bearer ";
    if (!header.empty() && header.rfind(kBearer, 0) == 0 && header.size() > kBearer.size()) {
        token = header.substr(kBearer.size());
    } else if (!header.empty() && header.find(' ') == std::string::npos) {
        // 兼容直接传裸 token 的情况（无前缀）
        token = header;
    }
    if (token.empty()) {
        result.code = 401;
        result.err = "未提供访问令牌！";
        return result;
    }

    // 验证 Token 的签名是否有效并提取用户 ID
    std::string uid_str;
    if (!VerifyJwt(token, &uid_str)) {
        result.code = 401;
        result.err = "无效的访问令牌！";
        return result;
    }

    // 检查 Token 是否已过期
    if (IsJwtExpired(token)) {
        result.code = 401;
        result.err = "访问令牌已过期！";
        return result;
    }

    result.data = std::stoull(uid_str);
    result.ok = true;
    return result;
}

PasswordResult DecryptPassword(const std::string& encrypted_password, std::string& out_plaintext) {
    PasswordResult result;

    // Base64 解码
    std::string cipher_bin = CIM::base64decode(encrypted_password);
    if (cipher_bin.empty()) {
        result.code = 400;
        result.err = "密码解码失败！";
        return result;
    }
    // 私钥解密
    auto cm = CIM::CryptoModule::Get();
    if (!cm || !cm->isReady()) {
        result.code = 500;
        result.err = "密钥模块未加载！";
        return result;
    }
    if (!cm->PrivateDecrypt(cipher_bin, out_plaintext)) {
        result.code = 400;
        result.err = "密码解密失败！";
        return result;
    }
    result.ok = true;
    return result;
}

CIM::http::HttpStatus ToHttpStatus(const int code) {
    switch (code) {
        case 400:
            return CIM::http::HttpStatus::BAD_REQUEST;  // 请求参数错误
        case 401:
            return CIM::http::HttpStatus::UNAUTHORIZED;  // 未认证/Token无效
        case 403:
            return CIM::http::HttpStatus::FORBIDDEN;  // 权限不足
        case 404:
            return CIM::http::HttpStatus::NOT_FOUND;  // 资源不存在
        case 405:
            return CIM::http::HttpStatus::METHOD_NOT_ALLOWED;  // 方法不允许
        case 406:
            return CIM::http::HttpStatus::NOT_ACCEPTABLE;  // 不可接受
        case 408:
            return CIM::http::HttpStatus::REQUEST_TIMEOUT;  // 请求超时
        case 409:
            return CIM::http::HttpStatus::CONFLICT;  // 冲突（如重复）
        case 410:
            return CIM::http::HttpStatus::GONE;  // 资源已删除
        case 413:
            return CIM::http::HttpStatus::PAYLOAD_TOO_LARGE;  // 请求体过大
        case 415:
            return CIM::http::HttpStatus::UNSUPPORTED_MEDIA_TYPE;  // 媒体类型不支持
        case 422:
            return CIM::http::HttpStatus::UNPROCESSABLE_ENTITY;  // 语义错误
        case 429:
            return CIM::http::HttpStatus::TOO_MANY_REQUESTS;  // 请求过多
        case 500:
            return CIM::http::HttpStatus::INTERNAL_SERVER_ERROR;  // 服务端错误
        case 501:
            return CIM::http::HttpStatus::NOT_IMPLEMENTED;  // 未实现
        case 502:
            return CIM::http::HttpStatus::BAD_GATEWAY;  // 网关错误
        case 503:
            return CIM::http::HttpStatus::SERVICE_UNAVAILABLE;  // 服务不可用
        case 504:
            return CIM::http::HttpStatus::GATEWAY_TIMEOUT;  // 网关超时
        default:
            return CIM::http::HttpStatus::INTERNAL_SERVER_ERROR;
    }
}
}  // namespace CIM
