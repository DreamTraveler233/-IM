#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iterator>

#include "api/upload_api_module.hpp"
#include "common/common.hpp"
#include "config/config.hpp"
#include "http/http_server.hpp"
#include "http/multipart/multipart_parser.hpp"
#include "system/application.hpp"
#include "system/env.hpp"
#include "util/json_util.hpp"
#include "util/string_util.hpp"

namespace IM::api {

static auto g_logger = IM_LOG_NAME("root");

static auto g_temp_base_dir =
    IM::Config::Lookup<std::string>("media.temp_base_dir", std::string("data/uploads/tmp"));

UploadApiModule::UploadApiModule(IM::domain::service::IMediaService::Ptr media_service,
                                 IM::http::multipart::MultipartParser::ptr parser)
    : Module("api.upload", "0.1.0", "builtin"),
      m_media_service(std::move(media_service)),
      m_parser(std::move(parser)) {}
bool UploadApiModule::onServerReady() {
    std::vector<IM::TcpServer::ptr> httpServers;
    if (!IM::Application::GetInstance()->getServer("http", httpServers)) {
        IM_LOG_WARN(g_logger) << "no http servers found when registering upload routes";
        return true;
    }

    // 初始化媒体服务的临时上传清理定时器
    m_media_service->InitTempCleanupTimer();

    for (auto& s : httpServers) {
        auto http = std::dynamic_pointer_cast<IM::http::HttpServer>(s);
        if (!http) continue;
        auto dispatch = http->getServletDispatch();

        dispatch->addServlet("/api/v1/upload/init-multipart",
                             [this](IM::http::HttpRequest::ptr req, IM::http::HttpResponse::ptr res,
                                    IM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 Json::Value body;
                                 std::string file_name;
                                 uint64_t file_size = 0;

                                 if (ParseBody(req->getBody(), body)) {
                                     file_name = IM::JsonUtil::GetString(body, "file_name");
                                     file_size = IM::JsonUtil::GetUint64(body, "file_size");
                                 }

                                 if (file_name.empty() || file_size == 0) {
                                     res->setStatus(ToHttpStatus(400));
                                     res->setBody(Error(400, "invalid params"));
                                     return 0;
                                 }

                                 auto uid_ret = GetUidFromToken(req, res);
                                 if (!uid_ret.ok) {
                                     res->setStatus(ToHttpStatus(uid_ret.code));
                                     res->setBody(Error(uid_ret.code, uid_ret.err));
                                     return 0;
                                 }

                                 std::string upload_id;
                                 uint32_t shard_size = 0;
                                 auto init_res = m_media_service->InitMultipartUpload(
                                     uid_ret.data, file_name, file_size);
                                 if (!init_res.ok) {
                                     res->setStatus(ToHttpStatus(init_res.code));
                                     res->setBody(Error(init_res.code, init_res.err));
                                     return 0;
                                 }
                                 upload_id = init_res.data.upload_id;
                                 shard_size = init_res.data.shard_size;

                                 Json::Value data;
                                 data["upload_id"] = upload_id;
                                 data["shard_size"] = shard_size;
                                 res->setBody(Ok(data));
                                 return 0;
                             });

        dispatch->addServlet("/api/v1/upload/multipart", [this](IM::http::HttpRequest::ptr req,
                                                                IM::http::HttpResponse::ptr res,
                                                                IM::http::HttpSession::ptr) {
            res->setHeader("Content-Type", "application/json");

            const std::string content_type_hdr2 = req->getHeader("Content-Type", "");
            const std::string transfer_encoding_hdr2 = req->getHeader("Transfer-Encoding", "");
            const std::string content_length_hdr2 = req->getHeader("Content-Length", "");
            size_t body_len2 = req->getBody().size();
            IM_LOG_DEBUG(g_logger)
                << "Content-Type header: '" << content_type_hdr2 << "' Transfer-Encoding: '"
                << transfer_encoding_hdr2 << "' Content-Length: '" << content_length_hdr2
                << "' body_size=" << body_len2;
            if (!transfer_encoding_hdr2.empty()) {
                std::string te2 = transfer_encoding_hdr2;
                for (auto& c : te2) c = static_cast<char>(::tolower((unsigned char)c));
                if (te2.find("chunked") != std::string::npos) {
                    IM_LOG_WARN(g_logger)
                        << "chunked Transfer-Encoding not supported for request bodies";
                    res->setStatus(ToHttpStatus(400));
                    res->setBody(Error(400, "Transfer-Encoding: chunked not supported"));
                    return 0;
                }
            }

            // Debugging: log Content-Type header and request body size to help diagnose parsing issues
            const std::string content_type_hdr = req->getHeader("Content-Type", "");
            const std::string transfer_encoding_hdr = req->getHeader("Transfer-Encoding", "");
            const std::string content_length_hdr = req->getHeader("Content-Length", "");
            size_t body_len = req->getBody().size();
            IM_LOG_DEBUG(g_logger)
                << "Content-Type header: '" << content_type_hdr << "' Transfer-Encoding: '"
                << transfer_encoding_hdr << "' Content-Length: '" << content_length_hdr
                << "' body_size=" << body_len;

            // If the request uses chunked transfer encoding, we do not currently support it
            // on the server side for request bodies; return a helpful error so client can
            // adjust behavior (e.g., provide Content-Length or avoid chunked encoding)
            if (!transfer_encoding_hdr.empty()) {
                std::string te = transfer_encoding_hdr;
                for (auto& c : te) c = static_cast<char>(::tolower((unsigned char)c));
                if (te.find("chunked") != std::string::npos) {
                    IM_LOG_WARN(g_logger)
                        << "chunked Transfer-Encoding not supported for request bodies";
                    res->setStatus(ToHttpStatus(400));
                    res->setBody(Error(400, "Transfer-Encoding: chunked not supported"));
                    return 0;
                }
            }

            // Parse multipart/form-data using infra parser
            std::vector<IM::http::multipart::Part> parts;
            std::string parse_err;
            auto parser = m_parser;
            std::string base_tmp_dir = IM::EnvMgr::GetInstance()->getAbsoluteWorkPath(
                IM::Config::Lookup<std::string>("media.temp_base_dir",
                                                std::string("data/uploads/tmp"))
                    ->getValue());
            if (!parser->Parse(req->getBody(), req->getHeader("Content-Type", ""), base_tmp_dir,
                               parts, &parse_err)) {
                res->setStatus(ToHttpStatus(400));
                res->setBody(Error(400, parse_err.empty() ? "parse multipart failed" : parse_err));
                return 0;
            }

            if (parts.empty()) {
                IM_LOG_INFO(g_logger) << "Parsed multipart parts count=0; Content-Type='"
                                      << req->getHeader("Content-Type", "")
                                      << "' body_size=" << req->getBody().size();
                res->setStatus(ToHttpStatus(400));
                res->setBody(Error(400,
                                   "no multipart parts parsed; ensure Content-Type "
                                   "multipart/form-data and request body not empty"));
                return 0;
            }

            if (parts.empty()) {
                // parts empty -> likely missing or malformed Content-Type or empty body
                IM_LOG_INFO(g_logger) << "Parsed multipart parts count=0; Content-Type='"
                                      << content_type_hdr2 << "' body_size=" << body_len2;
                res->setStatus(ToHttpStatus(400));
                res->setBody(Error(400,
                                   "no multipart parts parsed; ensure Content-Type "
                                   "multipart/form-data and request body not empty"));
                return 0;
            }

            IM_LOG_INFO(g_logger) << "Parsed multipart parts count=" << parts.size();
            for (const auto& p : parts) {
                IM_LOG_DEBUG(g_logger) << "part name=" << p.name << " filename=" << p.filename
                                       << " content_type=" << p.content_type << " size=" << p.size;
            }

            std::string upload_id;
            uint32_t split_index = 0;
            uint32_t split_num = 0;
            std::string file_temp_path;

            for (const auto& p : parts) {
                if (p.name == "upload_id")
                    upload_id = p.data;
                else if (p.name == "split_index")
                    split_index = std::stoul(p.data);
                else if (p.name == "split_num")
                    split_num = std::stoul(p.data);
                else if (p.name == "file") {
                    if (!p.temp_file.empty()) {
                        file_temp_path = p.temp_file;
                    } else if (!p.data.empty()) {
                        // write in-memory data to base temp dir
                        file_temp_path =
                            base_tmp_dir + "/parser_inmem_" + IM::random_string(8) + ".part";
                        std::ofstream ofs(file_temp_path, std::ios::binary | std::ios::trunc);
                        if (ofs) {
                            ofs.write(p.data.c_str(), p.data.size());
                            ofs.close();
                        } else {
                            IM_LOG_ERROR(g_logger)
                                << "write in-memory data to tmp file failed: " << file_temp_path;
                            file_temp_path.clear();
                        }
                    }
                }
                // 如果 part 没有显式 name='file'，但具有 filename，则回退使用该 part
                else if (p.filename.size() > 0 && file_temp_path.empty()) {
                    if (!p.temp_file.empty()) {
                        file_temp_path = p.temp_file;
                    } else if (!p.data.empty()) {
                        file_temp_path =
                            base_tmp_dir + "/parser_inmem_" + IM::random_string(8) + ".part";
                        std::ofstream ofs(file_temp_path, std::ios::binary | std::ios::trunc);
                        if (ofs) {
                            ofs.write(p.data.c_str(), p.data.size());
                            ofs.close();
                        } else {
                            IM_LOG_ERROR(g_logger)
                                << "write in-memory data to tmp file failed: " << file_temp_path;
                            file_temp_path.clear();
                        }
                    }
                }
            }

            if (upload_id.empty() || file_temp_path.empty()) {
                IM_LOG_WARN(g_logger)
                    << "multipart upload missing param upload_id?=" << upload_id.empty()
                    << " file?=" << file_temp_path.empty() << " parts_count=" << parts.size();
                res->setStatus(ToHttpStatus(400));
                res->setBody(Error(400, "missing params"));
                return 0;
            }

            // 鉴权
            auto uid_ret = GetUidFromToken(req, res);
            if (!uid_ret.ok) {
                res->setStatus(ToHttpStatus(uid_ret.code));
                res->setBody(Error(uid_ret.code, uid_ret.err));
                return 0;
            }

            // Ensure part file resides in the session temp_path (move if necessary)
            std::string part_path = file_temp_path;
            std::string session_tmp = m_media_service->GetUploadTempPath(upload_id);
            if (!session_tmp.empty()) {
                std::string final_part_path = session_tmp + "/part_" + std::to_string(split_index);
                if (!IM::FSUtil::Mv(part_path, final_part_path)) {
                    // fallback to copy
                    std::ifstream ifs(part_path, std::ios::binary);
                    if (ifs) {
                        std::ofstream ofs(final_part_path, std::ios::binary | std::ios::trunc);
                        if (ofs) {
                            ofs << ifs.rdbuf();
                            ofs.close();
                        }
                    }
                    IM::FSUtil::Unlink(part_path);
                }
                part_path = final_part_path;
            }

            auto up_res = m_media_service->UploadPart(upload_id, split_index, split_num, part_path);
            if (!up_res.ok) {
                res->setStatus(ToHttpStatus(up_res.code));
                res->setBody(Error(up_res.code, up_res.err));
                return 0;
            }
            bool completed = up_res.data;

            Json::Value data;
            data["is_completed"] = completed;
            if (completed) {
                IM::model::MediaFile media;
                // 首先查找 upload session 对应的媒体记录
                auto gf_by_upload_res = m_media_service->GetMediaFileByUploadId(upload_id);
                if (gf_by_upload_res.ok) {
                    auto gf_res = m_media_service->GetMediaFile(gf_by_upload_res.data.id);
                    if (gf_res.ok) {
                        data["file_id"] = gf_res.data.id;
                        data["url"] = gf_res.data.url;
                    }
                }
            }

            res->setBody(Ok(data));
            return 0;
        });

        dispatch->addServlet("/api/v1/upload/media-file", [this](IM::http::HttpRequest::ptr req,
                                                                 IM::http::HttpResponse::ptr res,
                                                                 IM::http::HttpSession::ptr) {
            res->setHeader("Content-Type", "application/json");

            std::vector<IM::http::multipart::Part> parts;
            std::string parse_err;
            auto parser = m_parser;
            std::string base_tmp_dir2 =
                IM::EnvMgr::GetInstance()->getAbsoluteWorkPath(g_temp_base_dir->getValue());
            if (!parser->Parse(req->getBody(), req->getHeader("Content-Type", ""), base_tmp_dir2,
                               parts, &parse_err)) {
                res->setStatus(ToHttpStatus(400));
                res->setBody(Error(400, parse_err.empty() ? "parse multipart failed" : parse_err));
                return 0;
            }

            if (parts.empty()) {
                IM_LOG_WARN(g_logger) << "no file part found in multipart upload, parts_count=0";
            }

            std::string file_data;
            std::string file_name;
            std::string file_content_type;

            for (const auto& p : parts) {
                if (p.name == "file") {
                    if (!p.data.empty())
                        file_data = p.data;
                    else if (!p.temp_file.empty()) {
                        std::ifstream ifs(p.temp_file, std::ios::binary);
                        if (ifs) {
                            std::string buf((std::istreambuf_iterator<char>(ifs)),
                                            std::istreambuf_iterator<char>());
                            file_data = std::move(buf);
                        }
                    }
                    file_name = p.filename;
                    file_content_type = p.content_type;
                    break;
                }
                // 回退：若没有显式 name='file'，则使用第一个看起来是文件的 part
                if (p.filename.size() > 0 && file_data.empty()) {
                    if (!p.data.empty())
                        file_data = p.data;
                    else if (!p.temp_file.empty()) {
                        std::ifstream ifs(p.temp_file, std::ios::binary);
                        if (ifs) {
                            std::string buf((std::istreambuf_iterator<char>(ifs)),
                                            std::istreambuf_iterator<char>());
                            file_data = std::move(buf);
                        }
                    }
                    file_name = p.filename;
                    file_content_type = p.content_type;
                    // 此处不提前 break，优先使用显式名为 'file' 的 part
                }
            }

            if (file_data.empty()) {
                IM_LOG_WARN(g_logger)
                    << "no file part found in multipart upload, parts_count=" << parts.size();
                res->setStatus(ToHttpStatus(400));
                res->setBody(Error(400, "missing file"));
                return 0;
            }
            if (file_name.empty()) file_name = "unknown";

            auto uid_ret = GetUidFromToken(req, res);
            if (!uid_ret.ok) {
                res->setStatus(ToHttpStatus(uid_ret.code));
                res->setBody(Error(uid_ret.code, uid_ret.err));
                return 0;
            }

            // 验证 MIME 类型（如头像上传）：仅允许图片
            std::vector<std::string> allowed_mimes = {"image/png", "image/jpg", "image/jpeg",
                                                      "image/webp", "image/gif"};
            bool ok_mime = false;
            if (!file_content_type.empty()) {
                // 在任何 ';' 参数之前提取基础 MIME 类型（例如 'image/jpeg; charset=UTF-8'）
                size_t semicolon = file_content_type.find(';');
                std::string base_mime = file_content_type;
                if (semicolon != std::string::npos)
                    base_mime = file_content_type.substr(0, semicolon);
                // 去除首尾空白
                while (!base_mime.empty() && base_mime.front() == ' ')
                    base_mime.erase(base_mime.begin());
                while (!base_mime.empty() && base_mime.back() == ' ') base_mime.pop_back();
                for (const auto& m : allowed_mimes) {
                    if (base_mime == m) {
                        ok_mime = true;
                        break;
                    }
                }
            }
            // 回退到扩展名检查
            if (!ok_mime) {
                std::string lower = file_name;
                for (auto& c : lower) c = std::tolower(c);
                if (lower.size() > 4) {
                    if (lower.find(".png") != std::string::npos ||
                        lower.find(".jpg") != std::string::npos ||
                        lower.find(".jpeg") != std::string::npos ||
                        lower.find(".webp") != std::string::npos ||
                        lower.find(".gif") != std::string::npos) {
                        ok_mime = true;
                    }
                }
            }

            if (!ok_mime) {
                res->setStatus(ToHttpStatus(400));
                res->setBody(Error(400, "invalid file type, only images allowed"));
                return 0;
            }

            IM::model::MediaFile media;
            auto upload_res = m_media_service->UploadFile(uid_ret.data, file_name, file_data);
            if (!upload_res.ok) {
                res->setStatus(ToHttpStatus(upload_res.code));
                res->setBody(Error(upload_res.code, upload_res.err));
                return 0;
            }
            media = upload_res.data;

            Json::Value data;
            data["id"] = media.id;
            data["src"] = media.url;
            res->setBody(Ok(data));
            return 0;
        });
    }
    return true;
}

}  // namespace IM::api
