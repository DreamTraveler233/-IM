#include "api/user_api_module.hpp"

#include "base/macro.hpp"
#include "common/common.hpp"
#include "config/config.hpp"
#include "http/http_server.hpp"
#include "system/application.hpp"
#include "util/util.hpp"

namespace IM::api {

static auto g_logger = IM_LOG_NAME("root");

// JWT过期时间(秒)
static auto g_jwt_expires_in =
    IM::Config::Lookup<uint32_t>("auth.jwt.expires_in", 3600, "jwt expires in seconds");

UserApiModule::UserApiModule(const IM::domain::service::IUserService::Ptr& user_service)
    : Module("api.user", "0.1.0", "builtin"), m_user_service(std::move(user_service)) {}

bool UserApiModule::onServerReady() {
    std::vector<IM::TcpServer::ptr> httpServers;
    if (!IM::Application::GetInstance()->getServer("http", httpServers)) {
        IM_LOG_WARN(g_logger) << "no http servers found when registering user routes";
        return true;
    }

    for (auto& s : httpServers) {
        auto http = std::dynamic_pointer_cast<IM::http::HttpServer>(s);
        if (!http) continue;
        auto dispatch = http->getServletDispatch();

        /*用户相关接口*/
        dispatch->addServlet("/api/v1/user/detail",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");

                                 // 解析Token获取用户ID
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }

                                 // 加载用户信息
                                 auto result = m_user_service->LoadUserInfo(uid_result.data);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }

                                 // 构造响应并返回
                                 Json::Value data;
                                 data["id"] = result.data.id;
                                 data["mobile"] = result.data.mobile;
                                 data["nickname"] = result.data.nickname;
                                 data["email"] = result.data.email;
                                 data["gender"] = result.data.gender;
                                 data["motto"] = result.data.motto;
                                 data["avatar"] = result.data.avatar;
                                 data["birthday"] = result.data.birthday;
                                 res->setBody(Ok(data));
                                 return 0;
                             });

        /*用户信息更新接口*/
        dispatch->addServlet("/api/v1/user/detail-update",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");

                                 // 提取请求字段
                                 std::string nickname, avatar, motto, birthday;
                                 uint32_t gender = 0;
                                 Json::Value body;
                                 if (IM::ParseBody(req->getBody(), body)) {
                                     nickname = IM::JsonUtil::GetString(body, "nickname");
                                     avatar = IM::JsonUtil::GetString(body, "avatar");
                                     motto = IM::JsonUtil::GetString(body, "motto");
                                     gender = IM::JsonUtil::GetUint32(body, "gender");
                                     birthday = IM::JsonUtil::GetString(body, "birthday");
                                 }

                                 // 解析Token获取用户ID
                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }

                                 // 更新用户信息
                                 auto result = m_user_service->UpdateUserInfo(
                                     uid_result.data, nickname, avatar, motto, gender, birthday);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }

                                 res->setBody(Ok());
                                 return 0;
                             });

        /*用户邮箱更新接口*/
        dispatch->addServlet("/api/v1/user/email-update", [this](IM::http::HttpRequest::ptr req,
                                                                 IM::http::HttpResponse::ptr res,
                                                                 IM::http::HttpSession::ptr) {
            res->setHeader("Content-Type", "application/json");

            auto uid_result = GetUidFromToken(req, res);
            if (!uid_result.ok) {
                res->setStatus(ToHttpStatus(uid_result.code));
                res->setBody(Error(uid_result.code, uid_result.err));
                return 0;
            }

            std::string new_email, password, email_code;
            Json::Value body;
            if (IM::ParseBody(req->getBody(), body)) {
                new_email = IM::JsonUtil::GetString(body, "email");
                password = IM::JsonUtil::GetString(body, "password");
                email_code = IM::JsonUtil::GetString(body, "code");
            }

            auto update_result =
                m_user_service->UpdateEmail(uid_result.data, password, new_email, email_code);
            if (!update_result.ok) {
                res->setStatus(ToHttpStatus(update_result.code));
                res->setBody(Error(update_result.code, update_result.err));
                return 0;
            }

            res->setBody(Ok());
            return 0;
        });

        /*用户手机更新接口*/
        dispatch->addServlet("/api/v1/user/mobile-update", [this](IM::http::HttpRequest::ptr req,
                                                                  IM::http::HttpResponse::ptr res,
                                                                  IM::http::HttpSession::ptr) {
            res->setHeader("Content-Type", "application/json");

            auto uid_result = GetUidFromToken(req, res);
            if (!uid_result.ok) {
                res->setStatus(ToHttpStatus(uid_result.code));
                res->setBody(Error(uid_result.code, uid_result.err));
                return 0;
            }

            std::string new_mobile, password, sms_code;
            Json::Value body;
            if (IM::ParseBody(req->getBody(), body)) {
                new_mobile = IM::JsonUtil::GetString(body, "mobile");
                password = IM::JsonUtil::GetString(body, "password");
                sms_code = IM::JsonUtil::GetString(body, "sms_code");
            }

            auto update_result =
                m_user_service->UpdateMobile(uid_result.data, password, new_mobile, sms_code);
            if (!update_result.ok) {
                res->setStatus(ToHttpStatus(update_result.code));
                res->setBody(Error(update_result.code, update_result.err));
                return 0;
            }

            res->setBody(Ok());
            return 0;
        });

        /*用户密码更新接口*/
        dispatch->addServlet("/api/v1/user/password-update", [this](IM::http::HttpRequest::ptr req,
                                                                    IM::http::HttpResponse::ptr res,
                                                                    IM::http::HttpSession::ptr) {
            res->setHeader("Content-Type", "application/json");

            std::string old_password, new_password;
            Json::Value body;
            if (IM::ParseBody(req->getBody(), body)) {
                old_password = IM::JsonUtil::GetString(body, "old_password");
                new_password = IM::JsonUtil::GetString(body, "new_password");
            }

            auto uid_result = GetUidFromToken(req, res);
            if (!uid_result.ok) {
                res->setStatus(ToHttpStatus(uid_result.code));
                res->setBody(Error(uid_result.code, uid_result.err));
                return 0;
            }

            auto result =
                m_user_service->UpdatePassword(uid_result.data, old_password, new_password);
            if (!result.ok) {
                res->setStatus(ToHttpStatus(result.code));
                res->setBody(Error(result.code, result.err));
                return 0;
            }

            res->setBody(Ok());
            return 0;
        });

        dispatch->addServlet("/api/v1/user/setting/save", [this](IM::http::HttpRequest::ptr req,
                                                                 IM::http::HttpResponse::ptr res,
                                                                 IM::http::HttpSession::ptr) {
            res->setHeader("Content-Type", "application/json");

            std::string theme_mode, theme_bag_img, theme_color, notify_cue_tone,
                keyboard_event_notify;
            Json::Value body;
            if (IM::ParseBody(req->getBody(), body)) {
                theme_mode = IM::JsonUtil::GetString(body, "theme_mode");
                theme_bag_img = IM::JsonUtil::GetString(body, "theme_bag_img");
                theme_color = IM::JsonUtil::GetString(body, "theme_color");
                notify_cue_tone = IM::JsonUtil::GetString(body, "notify_cue_tone");
                keyboard_event_notify = IM::JsonUtil::GetString(body, "keyboard_event_notify");
            }

            auto uid_result = GetUidFromToken(req, res);
            if (!uid_result.ok) {
                res->setStatus(ToHttpStatus(uid_result.code));
                res->setBody(Error(uid_result.code, uid_result.err));
                return 0;
            }

            auto save_result =
                m_user_service->SaveConfigInfo(uid_result.data, theme_mode, theme_bag_img,
                                               theme_color, notify_cue_tone, keyboard_event_notify);
            if (!save_result.ok) {
                res->setStatus(ToHttpStatus(save_result.code));
                res->setBody(Error(save_result.code, save_result.err));
                return 0;
            }

            res->setBody(Ok());
            return 0;
        });

        /*用户设置接口*/
        dispatch->addServlet("/api/v1/user/setting", [this](IM::http::HttpRequest::ptr req,
                                                            IM::http::HttpResponse::ptr res,
                                                            IM::http::HttpSession::ptr) {
            res->setHeader("Content-Type", "application/json");

            auto uid_result = GetUidFromToken(req, res);
            if (!uid_result.ok) {
                res->setStatus(ToHttpStatus(uid_result.code));
                res->setBody(Error(uid_result.code, uid_result.err));
                return 0;
            }

            // 加载用户信息简版
            auto user_info_result = m_user_service->LoadUserInfoSimple(uid_result.data);
            if (!user_info_result.ok) {
                res->setStatus(ToHttpStatus(user_info_result.code));
                res->setBody(Error(user_info_result.code, user_info_result.err));
                return 0;
            }

            // 加载用户设置
            auto config_info_result = m_user_service->LoadConfigInfo(uid_result.data);
            if (!config_info_result.ok) {
                res->setStatus(ToHttpStatus(config_info_result.code));
                res->setBody(Error(config_info_result.code, config_info_result.err));
                return 0;
            }

            Json::Value user_info;
            user_info["uid"] = user_info_result.data.uid;
            user_info["nickname"] = user_info_result.data.nickname;
            user_info["avatar"] = user_info_result.data.avatar;
            user_info["motto"] = user_info_result.data.motto;
            user_info["gender"] = user_info_result.data.gender;
            user_info["is_qiye"] = user_info_result.data.is_qiye;
            user_info["mobile"] = user_info_result.data.mobile;
            user_info["email"] = user_info_result.data.email;
            Json::Value setting;
            setting["theme_mode"] = config_info_result.data.theme_mode;
            setting["theme_bag_img"] = config_info_result.data.theme_bag_img;
            setting["theme_color"] = config_info_result.data.theme_color;
            setting["notify_cue_tone"] = config_info_result.data.notify_cue_tone;
            setting["keyboard_event_notify"] = config_info_result.data.keyboard_event_notify;
            Json::Value data;
            data["user_info"] = user_info;
            data["setting"] = setting;
            res->setBody(Ok(data));
            return 0;
        });
        /*登录接口*/
        dispatch->addServlet("/api/v1/auth/login", [this](IM::http::HttpRequest::ptr req,
                                                          IM::http::HttpResponse::ptr res,
                                                          IM::http::HttpSession::ptr session) {
            res->setHeader("Content-Type", "application/json");

            std::string mobile, password, platform;
            Json::Value body;
            if (ParseBody(req->getBody(), body)) {
                mobile = IM::JsonUtil::GetString(body, "mobile");
                password = IM::JsonUtil::GetString(body, "password");
                platform = IM::JsonUtil::GetString(body, "platform");
            }

            // 执行登陆业务
            auto result = m_user_service->Authenticate(mobile, password, platform);

            /*只要用户存在，无论鉴权成功还是失败都要记录登录日志*/
            std::string err;
            if (result.data.id != 0) {
                auto LogLogin_res = m_user_service->LogLogin(result, platform, session);
                if (!LogLogin_res.ok) {
                    res->setStatus(ToHttpStatus(LogLogin_res.code));
                    res->setBody(Error(LogLogin_res.code, LogLogin_res.err));
                    return 0;
                }
            }
            if (!result.ok) {
                res->setStatus(ToHttpStatus(result.code));
                res->setBody(Error(result.code, result.err));
                return 0;
            }

            /* 签发JWT */
            auto token_result =
                SignJwt(std::to_string(result.data.id), g_jwt_expires_in->getValue());
            if (!token_result.ok) {
                res->setStatus(ToHttpStatus(token_result.code));
                res->setBody(Error(token_result.code, token_result.err));
                return 0;
            }

            // 更新在线状态为在线
            auto goOnline_res = m_user_service->GoOnline(result.data.id);
            if (!goOnline_res.ok) {
                res->setStatus(ToHttpStatus(goOnline_res.code));
                res->setBody(Error(goOnline_res.code, goOnline_res.err));
                return 0;
            }

            Json::Value data;
            data["type"] = "Bearer";                   // token类型，固定值Bearer
            data["access_token"] = token_result.data;  // 访问令牌
            data["expires_in"] =
                static_cast<Json::UInt>(g_jwt_expires_in->getValue());  // 过期时间(秒)
            res->setBody(Ok(data));
            return 0;
        });

        /*注册接口*/
        dispatch->addServlet("/api/v1/auth/register", [this](IM::http::HttpRequest::ptr req,
                                                             IM::http::HttpResponse::ptr res,
                                                             IM::http::HttpSession::ptr session) {
            res->setHeader("Content-Type", "application/json");

            std::string nickname, mobile, password, sms_code, platform;
            Json::Value body;
            if (ParseBody(req->getBody(), body)) {
                nickname = IM::JsonUtil::GetString(body, "nickname");
                mobile = IM::JsonUtil::GetString(body, "mobile");
                password = IM::JsonUtil::GetString(body, "password");
                sms_code = IM::JsonUtil::GetString(body, "sms_code");
                platform = IM::JsonUtil::GetString(body, "platform");
            }

            /* 注册用户 */
            auto result = m_user_service->Register(nickname, mobile, password, sms_code, platform);

            /*只要创建用户成功就记录登录日志*/
            std::string err;
            if (result.data.id != 0) {
                auto LogLogin_res = m_user_service->LogLogin(result, platform, session);
                if (!LogLogin_res.ok) {
                    res->setStatus(ToHttpStatus(LogLogin_res.code));
                    res->setBody(Error(LogLogin_res.code, LogLogin_res.err));
                    return 0;
                }
            }
            if (!result.ok) {
                res->setStatus(ToHttpStatus(result.code));
                res->setBody(Error(result.code, result.err));
                return 0;
            }

            /* 签发JWT */
            auto token_result =
                SignJwt(std::to_string(result.data.id), g_jwt_expires_in->getValue());
            if (!token_result.ok) {
                res->setStatus(ToHttpStatus(token_result.code));
                res->setBody(Error(token_result.code, token_result.err));
                return 0;
            }

            // 更新在线状态为在线
            auto goOnline_res = m_user_service->GoOnline(result.data.id);
            if (!goOnline_res.ok) {
                res->setStatus(ToHttpStatus(goOnline_res.code));
                res->setBody(Error(goOnline_res.code, goOnline_res.err));
                return 0;
            }

            Json::Value data;
            data["type"] = "Bearer";
            data["access_token"] = token_result.data;
            data["expires_in"] = static_cast<Json::UInt>(g_jwt_expires_in->getValue());
            res->setBody(Ok(data));
            return 0;
        });

        /*找回密码接口*/
        dispatch->addServlet("/api/v1/auth/forget", [this](IM::http::HttpRequest::ptr req,
                                                           IM::http::HttpResponse::ptr res,
                                                           IM::http::HttpSession::ptr session) {
            res->setHeader("Content-Type", "application/json");

            std::string mobile, password, sms_code;
            Json::Value body;
            if (ParseBody(req->getBody(), body)) {
                mobile = IM::JsonUtil::GetString(body, "mobile");
                password = IM::JsonUtil::GetString(body, "password");
                sms_code = IM::JsonUtil::GetString(body, "sms_code");
            }

            // 找回密码
            auto forgetResult = m_user_service->Forget(mobile, password, sms_code);
            if (!forgetResult.ok) {
                res->setStatus(ToHttpStatus(forgetResult.code));
                res->setBody(Error(forgetResult.code, forgetResult.err));
                return 0;
            }

            res->setBody(Ok());
            return 0;
        });

        /*获取 oauth2.0 跳转地址*/
        dispatch->addServlet("/api/v1/auth/oauth",
                             [this](IM::http::HttpRequest::ptr /*req*/, IM::http::HttpResponse::ptr res,
                                IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 res->setBody(Ok());
                                 return 0;
                             });

        /*绑定第三方登录接口*/
        dispatch->addServlet("/api/v1/auth/oauth/bind",
                             [this](IM::http::HttpRequest::ptr /*req*/, IM::http::HttpResponse::ptr res,
                                IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 res->setBody(Ok());
                                 return 0;
                             });

        /*第三方登录接口*/
        dispatch->addServlet("/api/v1/auth/oauth/login",
                             [this](IM::http::HttpRequest::ptr /*req*/, IM::http::HttpResponse::ptr res,
                                IM::http::HttpSession::ptr /*session*/) {
                                 res->setHeader("Content-Type", "application/json");
                                 res->setBody(Ok());
                                 return 0;
                             });
    }
    return true;
}

}  // namespace IM::api
