#pragma once

#include <string>
#include <vector>

namespace CIM {
class StringUtil {
   public:
    /**
         * @brief 判断字符串 str 是否以子串 sub 开头。
         * @param str 待判断的字符串
         * @param sub 前缀子串
         * @return true 如果 str 以 sub 开头
         * @return false 否则
         */
    static bool StartsWith(const std::string& str, const std::string& sub);

    /**
         * @brief 判断字符串 str 是否以子串 sub 结尾。
         * @param str 待判断的字符串
         * @param sub 后缀子串
         * @return true 如果 str 以 sub 结尾
         * @return false 否则
         */
    static bool EndsWith(const std::string& str, const std::string& sub);

    /**
         * @brief 获取文件路径字符串中的目录部分。
         *
         * @param path 文件路径
         * @return string 目录部分（不包含文件名），若无目录则返回"./"
         *
         * @par 示例
         * - 输入：/home/user/file.txt，返回：/home/user
         * - 输入：C:\data\test.txt，返回：C:\data
         * - 输入：file.txt，返回：./
         */
    static std::string FilePath(const std::string& path);

    /**
         * @brief 获取文件路径中的文件名和扩展名部分（即去掉目录，只保留最后的文件名部分）。
         *
         * @param path 文件路径
         * @return string 文件名及扩展名
         *
         * @par 示例
         * - 输入：/home/user/file.txt，返回：file.txt
         * - 输入：C:\data\test.log，返回：test.log
         * - 输入：file.txt，返回：file.txt
         * - 输入：/home/user/，返回：/home/user/
         */
    static std::string FileNameExt(const std::string& path);

    /**
         * @brief 获取文件路径中的文件名（不带扩展名）。
         *
         * @param path 文件路径
         * @return string 文件名（不含扩展名）
         *
         * @par 示例
         * - 输入：/home/user/file.txt，返回：file
         * - 输入：C:\data\test.log，返回：test
         * - 输入：file，返回：file
         * - 输入：.bashrc，返回：.bashrc
         */
    static std::string FileName(const std::string& path);

    /**
         * @brief 获取文件路径中的扩展名部分（包括点）。
         *
         * @param path 文件路径
         * @return string 扩展名（包含点），若无扩展名返回空字符串
         *
         * @par 示例
         * - 输入：/home/user/file.txt，返回：.txt
         * - 输入：C:\data\test.log，返回：.log
         * - 输入：file，返回：""
         * - 输入：.bashrc，返回：""
         */
    static std::string Extension(const std::string& path);

    /**
         * @brief 将一个字符串按指定分隔符切分成多个子串。
         *
         * @param str 待分割的字符串
         * @param delimiter 分隔符
         * @return vector<string> 分割后的子串数组
         *
         * @par 示例
         * - 输入：str = "a,b,c", delimiter = ","，返回：["a", "b", "c"]
         * - 输入：str = "a--b--c", delimiter = "--"，返回：["a", "b", "c"]
         * - 输入：str = "abc", delimiter = ","，返回：["abc"]
         * - 输入：str = ",a,b,", delimiter = ","，返回：["a", "b"]
         */
    static std::vector<std::string> SplitString(const std::string& str,
                                                const std::string& delimiter);

    static std::string Format(const char* fmt, ...);
    static std::string Formatv(const char* fmt, va_list ap);

    static std::string UrlEncode(const std::string& str, bool space_as_plus = true);
    static std::string UrlDecode(const std::string& str, bool space_as_plus = true);

    static std::string Trim(const std::string& str, const std::string& delimit = " \t\r\n");
    static std::string TrimLeft(const std::string& str, const std::string& delimit = " \t\r\n");
    static std::string TrimRight(const std::string& str, const std::string& delimit = " \t\r\n");

    static std::string WStringToString(const std::wstring& ws);
    static std::wstring StringToWString(const std::string& s);
};
}  // namespace CIM