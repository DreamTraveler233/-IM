#include <stdint.h>

#include <cstdarg>
#include <cstring>

#include "util/string_util.hpp"

namespace CIM {
bool StringUtil::StartsWith(const std::string& str, const std::string& sub) {
    // 如果字串为空，直接返回 true
    if (sub.empty()) {
        return true;
    }
    // 如果字符串为空或字符串比子串短，直接返回 false
    auto strLen = str.size();
    auto subLen = sub.size();
    if (str.empty() || strLen < subLen) {
        return false;
    }
    // 用 str 从下标 0 开始、长度为 subLen 的子串，与 sub 进行比较
    return str.compare(0, subLen, sub) == 0;
}

bool StringUtil::EndsWith(const std::string& str, const std::string& sub) {
    // 如果字串为空，直接返回 true
    if (sub.empty()) {
        return true;
    }
    // 如果字符串为空或字符串比子串短，直接返回 false
    auto strLen = str.size();
    auto subLen = sub.size();
    if (str.empty() || strLen < subLen) {
        return false;
    }
    // 用 str 从下标 0 开始、长度为 subLen 的子串，与 sub 进行比较
    return str.compare(strLen - subLen, subLen, sub) == 0;
}

std::string StringUtil::FilePath(const std::string& path) {
    auto pos = path.find_last_of("/\\");  // 查找最后一个'/'或'\'的位置（兼容Linux和Windows路径）
    if (pos != std::string::npos)         // 如果找到了分隔符
    {
        return path.substr(0, pos + 1);  // 返回从开头到分隔符前的所有字符（即目录部分）
    } else                               // 如果没有找到分隔符
    {
        return "./";  // 返回当前目录
    }
}

std::string StringUtil::FileNameExt(const std::string& path) {
    auto pos =
        path.find_last_of("/\\");  // 查找路径中最后一个'/'或'\'的位置（兼容Linux和Windows路径）
    if (pos != std::string::npos)  // 如果找到了分隔符
    {
        if (pos + 1 < path.size())  // 并且分隔符后还有内容
        {
            return path.substr(pos + 1);  // 返回分隔符后的所有内容（即文件名+扩展名）
        }
    }
    return path;  // 如果没有分隔符，直接返回原字符串（说明本身就是文件名）
}

std::string StringUtil::FileName(const std::string& path) {
    std::string file_name = FileNameExt(path);  // 先获取文件名（含扩展名）
    auto pos = file_name.find_last_of(".");     // 查找最后一个'.'的位置（即扩展名前的点）
    if (pos != std::string::npos)               // 如果找到了'.'
    {
        if (pos != 0)  // 并且'.'不是第一个字符（防止隐藏文件如 .bashrc）
        {
            return file_name.substr(0, pos);  // 返回从头到'.'前的内容（即文件名，不含扩展名）
        }
    }
    return file_name;  // 如果没有'.'，或'.'在第一个字符，直接返回整个文件名
}

std::string StringUtil::Extension(const std::string& path) {
    std::string file_name = FileNameExt(path);  // 先获取文件名（含扩展名）
    auto pos = file_name.find_last_of(".");     // 查找最后一个'.'的位置
    if (pos != std::string::npos)               // 如果找到了'.'
    {
        if (pos != 0 && pos + 1 < path.size())  // '.'不是第一个字符，且后面还有内容
        {
            return file_name.substr(pos);  // 返回从'.'开始到结尾的内容（即扩展名，包含点）
        }
    }
    return std::string();  // 没有扩展名则返回空字符串
}

std::vector<std::string> StringUtil::SplitString(const std::string& str,
                                                 const std::string& delimiter) {
    std::vector<std::string> result;  // 存放分割后的子串
    // 如果分隔符为空，直接返回空vector
    if (delimiter.empty()) {
        return result;
    }
    size_t last = 0;  // 上一次分割结束的位置
    size_t next = 0;  // 下一个分隔符出现的位置
    // 循环查找分隔符并分割
    while ((next = str.find(delimiter, last)) != std::string::npos) {
        if (next > last)  // 分隔符前有内容
        {
            // 截取从last到next之间的子串，加入结果
            result.emplace_back(str.substr(last, next - last));
        }
        // 更新last到下一个分隔符后面
        last = next + delimiter.size();
    }
    // 处理最后一段（如果last还没到末尾）
    if (last < str.size()) {
        result.emplace_back(str.substr(last));
    }
    return result;  // 返回所有分割结果
}

std::string StringUtil::Format(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    auto v = Formatv(fmt, ap);
    va_end(ap);
    return v;
}

std::string StringUtil::Formatv(const char* fmt, va_list ap) {
    char* buf = nullptr;
    auto len = vasprintf(&buf, fmt, ap);
    if (len == -1) {
        return "";
    }
    std::string ret(buf, len);
    free(buf);
    return ret;
}

static const char uri_chars[256] = {
    /* 0 */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    1,
    0,
    0,
    /* 64 */
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    1,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    1,
    0,
    /* 128 */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 192 */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

static const char xdigit_chars[256] = {
    0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    0, 0, 0, 0, 0, 0, 0, 10, 11, 12, 13, 14, 15, 0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0,
};

#define CHAR_IS_UNRESERVED(c) (uri_chars[(unsigned char)(c)])

//-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz~
std::string StringUtil::UrlEncode(const std::string& str, bool space_as_plus) {
    static const char* hexdigits = "0123456789ABCDEF";
    std::string* ss = nullptr;
    const char* end = str.c_str() + str.length();
    for (const char* c = str.c_str(); c < end; ++c) {
        if (!CHAR_IS_UNRESERVED(*c)) {
            if (!ss) {
                ss = new std::string;
                ss->reserve(str.size() * 1.2);
                ss->append(str.c_str(), c - str.c_str());
            }
            if (*c == ' ' && space_as_plus) {
                ss->append(1, '+');
            } else {
                ss->append(1, '%');
                ss->append(1, hexdigits[(uint8_t)*c >> 4]);
                ss->append(1, hexdigits[*c & 0xf]);
            }
        } else if (ss) {
            ss->append(1, *c);
        }
    }
    if (!ss) {
        return str;
    } else {
        std::string rt = *ss;
        delete ss;
        return rt;
    }
}

std::string StringUtil::UrlDecode(const std::string& str, bool space_as_plus) {
    std::string* ss = nullptr;
    const char* end = str.c_str() + str.length();
    for (const char* c = str.c_str(); c < end; ++c) {
        if (*c == '+' && space_as_plus) {
            if (!ss) {
                ss = new std::string;
                ss->append(str.c_str(), c - str.c_str());
            }
            ss->append(1, ' ');
        } else if (*c == '%' && (c + 2) < end && isxdigit(*(c + 1)) && isxdigit(*(c + 2))) {
            if (!ss) {
                ss = new std::string;
                ss->append(str.c_str(), c - str.c_str());
            }
            ss->append(1, (char)(xdigit_chars[(int)*(c + 1)] << 4 | xdigit_chars[(int)*(c + 2)]));
            c += 2;
        } else if (ss) {
            ss->append(1, *c);
        }
    }
    if (!ss) {
        return str;
    } else {
        std::string rt = *ss;
        delete ss;
        return rt;
    }
}

std::string StringUtil::Trim(const std::string& str, const std::string& delimit) {
    auto begin = str.find_first_not_of(delimit);
    if (begin == std::string::npos) {
        return "";
    }
    auto end = str.find_last_not_of(delimit);
    return str.substr(begin, end - begin + 1);
}

std::string StringUtil::TrimLeft(const std::string& str, const std::string& delimit) {
    auto begin = str.find_first_not_of(delimit);
    if (begin == std::string::npos) {
        return "";
    }
    return str.substr(begin);
}

std::string StringUtil::TrimRight(const std::string& str, const std::string& delimit) {
    auto end = str.find_last_not_of(delimit);
    if (end == std::string::npos) {
        return "";
    }
    return str.substr(0, end);
}

std::string StringUtil::WStringToString(const std::wstring& ws) {
    std::string str_locale = setlocale(LC_ALL, "");
    const wchar_t* wch_src = ws.c_str();
    size_t n_dest_size = wcstombs(NULL, wch_src, 0) + 1;
    char* ch_dest = new char[n_dest_size];
    memset(ch_dest, 0, n_dest_size);
    wcstombs(ch_dest, wch_src, n_dest_size);
    std::string str_result = ch_dest;
    delete[] ch_dest;
    setlocale(LC_ALL, str_locale.c_str());
    return str_result;
}

std::wstring StringUtil::StringToWString(const std::string& s) {
    std::string str_locale = setlocale(LC_ALL, "");
    const char* chSrc = s.c_str();
    size_t n_dest_size = mbstowcs(NULL, chSrc, 0) + 1;
    wchar_t* wch_dest = new wchar_t[n_dest_size];
    wmemset(wch_dest, 0, n_dest_size);
    mbstowcs(wch_dest, chSrc, n_dest_size);
    std::wstring wstr_result = wch_dest;
    delete[] wch_dest;
    setlocale(LC_ALL, str_locale.c_str());
    return wstr_result;
}
}  // namespace CIM