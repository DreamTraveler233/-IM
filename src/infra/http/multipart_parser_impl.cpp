#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "config/config.hpp"
#include "http/multipart/multipart_parser.hpp"
#include "system/env.hpp"
#include "util/util.hpp"

namespace IM {
namespace http {
namespace multipart {

static auto g_memory_threshold_conf = IM::Config::Lookup<size_t>(
    "media.multipart_memory_threshold", (size_t)(1024 * 1024), "multipart parser memory threshold");

static std::string Trim(const std::string& s) {
    size_t first = s.find_first_not_of(" \t");
    if (std::string::npos == first) return "";
    size_t last = s.find_last_not_of(" \t");
    return s.substr(first, (last - first + 1));
}

static std::string GetBoundary(const std::string& content_type) {
    size_t pos = content_type.find("boundary=");
    if (pos == std::string::npos) return "";
    std::string val = content_type.substr(pos + 9);
    
    size_t semicolon = val.find(';');
    if (semicolon != std::string::npos) {
        val = val.substr(0, semicolon);
    }

    while (!val.empty() && isspace((unsigned char)val.front())) val.erase(val.begin());
    while (!val.empty() && isspace((unsigned char)val.back())) val.pop_back();
    
    if (!val.empty() && val.front() == '"' && val.back() == '"') {
        val = val.substr(1, val.size() - 2);
    }
    return val;
}

class SimpleMultipartParser : public MultipartParser {
public:
    bool Parse(const std::string& body, const std::string& content_type,
               const std::string& temp_dir, std::vector<Part>& parts,
               std::string* err = nullptr) override {
        
        std::string boundary = GetBoundary(content_type);
        // IM_LOG_INFO(IM_LOG_NAME("root")) << "Parse start. Content-Type: " << content_type << ", Boundary: '" << boundary << "'";
        
        // Fallback: Sniff boundary if missing or if parsing fails later (though we check upfront here)
        if (boundary.empty()) {
             size_t first_crlf = body.find("\r\n");
             if (first_crlf != std::string::npos && first_crlf > 2) {
                 std::string line = body.substr(0, first_crlf);
                 if (line.substr(0, 2) == "--") {
                     boundary = line.substr(2);
                     IM_LOG_INFO(IM_LOG_NAME("root")) << "Sniffed boundary: " << boundary;
                 }
             }
        }

        if (boundary.empty()) {
            if (err) *err = "missing boundary";
            IM_LOG_ERROR(IM_LOG_NAME("root")) << "Missing boundary";
            return false;
        }

        std::string delimiter = "--" + boundary;
        // IM_LOG_INFO(IM_LOG_NAME("root")) << "Delimiter: '" << delimiter << "'";

        size_t boundary_count = 0;
        size_t search_pos = 0;
        while (true) {
            size_t idx = body.find(delimiter, search_pos);
            if (idx == std::string::npos) {
                break;
            }
            ++boundary_count;
            search_pos = idx + delimiter.size();
        }
        IM_LOG_INFO(IM_LOG_NAME("root")) << "Boundary occurrences: " << boundary_count
                                          << ", body size: " << body.size();
        
        size_t pos = body.find(delimiter);
        if (pos == std::string::npos) {
            if (err) *err = "boundary not found in body";
            IM_LOG_ERROR(IM_LOG_NAME("root")) << "Boundary not found in body. Body size: " << body.size();
            if (body.size() > 0) {
                 IM_LOG_DEBUG(IM_LOG_NAME("root")) << "Body start: " << body.substr(0, std::min<size_t>(100, body.size()));
            }
            return false;
        }

        // IM_LOG_INFO(IM_LOG_NAME("root")) << "Found first boundary at pos: " << pos;

        while (true) {
            size_t next_pos = body.find(delimiter, pos + delimiter.size());
            if (next_pos == std::string::npos) {
                //IM_LOG_INFO(IM_LOG_NAME("root")) << "No next boundary found after pos: " << pos;
                break;
            }

            // IM_LOG_INFO(IM_LOG_NAME("root")) << "Found next boundary at pos: " << next_pos;

            size_t start_part = pos + delimiter.size();
            
            // Check for end of multipart "--boundary--"
            if (start_part + 2 <= body.size() && body.substr(start_part, 2) == "--") {
                IM_LOG_INFO(IM_LOG_NAME("root")) << "Found end of multipart at pos: " << start_part;
                break;
            }
            
            // Skip CRLF after boundary
            if (start_part + 2 <= body.size() && body.substr(start_part, 2) == "\r\n") {
                start_part += 2;
            } else if (start_part + 1 <= body.size() && body[start_part] == '\n') {
                start_part += 1;
            } else {
                 IM_LOG_WARN(IM_LOG_NAME("root")) << "No CRLF after boundary at " << start_part;
            }

            // The part data ends before the next boundary's CRLF prefix
            size_t end_part = next_pos;
            if (end_part >= 2 && body.substr(end_part - 2, 2) == "\r\n") {
                end_part -= 2;
            } else if (end_part >= 1 && body[end_part - 1] == '\n') {
                end_part -= 1;
            }

            if (start_part >= end_part) {
                IM_LOG_WARN(IM_LOG_NAME("root")) << "Empty part or malformed? start=" << start_part << ", end=" << end_part;
                pos = next_pos;
                continue;
            }

            std::string part_content = body.substr(start_part, end_part - start_part);
            
            size_t header_sep = part_content.find("\r\n\r\n");
            size_t header_sep_len = 4;
            if (header_sep == std::string::npos) {
                header_sep = part_content.find("\n\n");
                header_sep_len = 2;
            }

            if (header_sep == std::string::npos) {
                IM_LOG_WARN(IM_LOG_NAME("root")) << "No header separator found in part";
                pos = next_pos;
                continue;
            }

            std::string header_str = part_content.substr(0, header_sep);
            std::string data_str = part_content.substr(header_sep + header_sep_len);
            
            //IM_LOG_INFO(IM_LOG_NAME("root")) << "Part found. Header size: " << header_str.size() << ", Data size: " << data_str.size();

            Part part;
            part.size = data_str.size();
            
            // Parse headers
            std::stringstream ss(header_str);
            std::string line;
            while (std::getline(ss, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.empty()) continue;

                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    std::string key = line.substr(0, colon);
                    std::string val = Trim(line.substr(colon + 1));
                    std::string key_lower = key;
                    std::transform(key_lower.begin(), key_lower.end(), key_lower.begin(), ::tolower);
                    
                    if (key_lower == "content-disposition") {
                        size_t name_pos = val.find("name=");
                        if (name_pos != std::string::npos) {
                            size_t q1 = val.find('"', name_pos);
                            if (q1 != std::string::npos) {
                                size_t q2 = val.find('"', q1 + 1);
                                if (q2 != std::string::npos) {
                                    part.name = val.substr(q1 + 1, q2 - q1 - 1);
                                }
                            }
                        }
                        size_t fn_pos = val.find("filename=");
                        if (fn_pos != std::string::npos) {
                            size_t q1 = val.find('"', fn_pos);
                            if (q1 != std::string::npos) {
                                size_t q2 = val.find('"', q1 + 1);
                                if (q2 != std::string::npos) {
                                    part.filename = val.substr(q1 + 1, q2 - q1 - 1);
                                }
                            }
                        }
                    } else if (key_lower == "content-type") {
                        part.content_type = val;
                    }
                }
            }

            size_t threshold = g_memory_threshold_conf->getValue();
            if (part.size > threshold) {
                 std::string tmp_fn = temp_dir + "/parser_" + IM::random_string(16) + ".part";
                 if (IM::FSUtil::Mkdir(temp_dir)) {
                     std::ofstream ofs(tmp_fn, std::ios::binary);
                     if (ofs) {
                        ofs.write(data_str.c_str(), data_str.size());
                        ofs.close();
                        part.temp_file = tmp_fn;
                     }
                 }
                 // If mkdir or write fails, keep in memory (or handle error)
                 if (part.temp_file.empty()) {
                     part.data = data_str;
                 }
            } else {
                part.data = data_str;
            }
            
            if (part.name.empty() && !part.filename.empty()) part.name = "file";
            parts.push_back(part);

            pos = next_pos;
        }

        return true;
    }
};

std::shared_ptr<MultipartParser> CreateMultipartParser() {
    return std::make_shared<SimpleMultipartParser>();
}

}  // namespace multipart
}  // namespace http
}  // namespace IM