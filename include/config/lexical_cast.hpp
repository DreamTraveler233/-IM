#ifndef __CIM_CONFIG_LEXICAL_CAST_HPP__
#define __CIM_CONFIG_LEXICAL_CAST_HPP__

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <cctype>
#include <list>
#include <map>
#include <set>
#include <unordered_set>

#include "log/logger_manager.hpp"

namespace CIM {
/**
     * 不同类型的数据转换
     * 主要思路：
     *     使用模板的全特化和偏特化满足不同类型的转换
     * 基本类型：使用 boost::lexical_cast 实现
     * 容器类型/自定义类型：使用 YAML 进行序列化和反序列化
     */

// 主模板，实现通用数据转换 F from_type  T to_type
template <class F, class T>
class LexicalCast {
   public:
    T operator()(const F& from) const { return boost::lexical_cast<T>(from); }
};

// 专门处理字符串到 bool 的转换，支持多种常见写法
template <>
class LexicalCast<std::string, bool> {
   public:
    bool operator()(const std::string& v) const {
        // 去除首尾空白并转小写
        auto begin = v.begin();
        auto end = v.end();
        while (begin != end && std::isspace(static_cast<unsigned char>(*begin))) ++begin;
        while (begin != end && std::isspace(static_cast<unsigned char>(*(end - 1)))) --end;
        std::string s(begin, end);
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (s == "1" || s == "true" || s == "yes" || s == "on") return true;
        if (s == "0" || s == "false" || s == "no" || s == "off") return false;
        // 回退到 boost 的默认行为（可能抛异常）
        return boost::lexical_cast<bool>(v);
    }
};

// 模板偏特化，实现字符串转化为vector
template <class T>
class LexicalCast<std::string, std::vector<T>> {
   public:
    std::vector<T> operator()(const std::string& v) const {
        // 将字符串解析为YAML节点， node 相当于一个数组
        YAML::Node node = YAML::Load(v);
        typename std::vector<T> vec;
        std::stringstream ss;
        for (size_t i = 0; i < node.size(); ++i) {
            ss.str("");     // 清空字符串流
            ss << node[i];  // 将节点元素输出到字符串流中（将YAML节点转化为字符串形式）
            // 将字符串转为 T 类型后添加到vector中
            vec.push_back(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

// 模板偏特化，实现vector转化为字符串
template <class T>
class LexicalCast<std::vector<T>, std::string> {
   public:
    std::string operator()(const std::vector<T>& v) const {
        YAML::Node node;
        for (auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// 模板偏特化，实现字符串转化为 list
template <class T>
class LexicalCast<std::string, std::list<T>> {
   public:
    std::list<T> operator()(const std::string& v) const {
        // 将字符串解析为YAML节点， node 相当于一个数组
        YAML::Node node = YAML::Load(v);
        typename std::list<T> list;
        std::stringstream ss;
        for (size_t i = 0; i < node.size(); ++i) {
            ss.str("");     // 清空字符串流
            ss << node[i];  // 将节点元素输出到字符串流中（将YAML节点转化为字符串形式）
            // 将字符串转为 T 类型后添加到 list 中
            list.push_back(LexicalCast<std::string, T>()(ss.str()));
        }
        return list;
    }
};

// 模板偏特化，实现 list 转化为字符串
template <class T>
class LexicalCast<std::list<T>, std::string> {
   public:
    std::string operator()(const std::list<T>& v) const {
        YAML::Node node;
        for (auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// 模板偏特化，实现字符串转化为 set
template <class T>
class LexicalCast<std::string, std::set<T>> {
   public:
    std::set<T> operator()(const std::string& v) const {
        // 将字符串解析为YAML节点， node 相当于一个数组
        YAML::Node node = YAML::Load(v);
        typename std::set<T> set;
        std::stringstream ss;
        for (size_t i = 0; i < node.size(); ++i) {
            ss.str("");     // 清空字符串流
            ss << node[i];  // 将节点元素输出到字符串流中（将YAML节点转化为字符串形式）
            // 将字符串转为 T 类型后添加到 set 中
            set.insert(LexicalCast<std::string, T>()(ss.str()));
        }
        return set;
    }
};

// 模板偏特化，实现 set 转化为字符串
template <class T>
class LexicalCast<std::set<T>, std::string> {
   public:
    std::string operator()(const std::set<T>& v) const {
        YAML::Node node;
        for (auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// 模板偏特化，实现字符串转化为 unordered_set
template <class T>
class LexicalCast<std::string, std::unordered_set<T>> {
   public:
    std::unordered_set<T> operator()(const std::string& v) const {
        // 将字符串解析为YAML节点， node 相当于一个数组
        YAML::Node node = YAML::Load(v);
        typename std::unordered_set<T> unordered_set;
        std::stringstream ss;
        for (size_t i = 0; i < node.size(); ++i) {
            ss.str("");     // 清空字符串流
            ss << node[i];  // 将节点元素输出到字符串流中（将YAML节点转化为字符串形式）
            // 将字符串转为 T 类型后添加到 unordered_set 中
            unordered_set.insert(LexicalCast<std::string, T>()(ss.str()));
        }
        return unordered_set;
    }
};

// 模板偏特化，实现 unordered_set 转化为字符串
template <class T>
class LexicalCast<std::unordered_set<T>, std::string> {
   public:
    std::string operator()(const std::unordered_set<T>& v) const {
        YAML::Node node;
        for (auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// 模板偏特化，实现字符串转化为 map
template <class T>
class LexicalCast<std::string, std::map<std::string, T>> {
   public:
    std::map<std::string, T> operator()(const std::string& v) const {
        // 将字符串解析为YAML节点， node 相当于一个数组
        YAML::Node node = YAML::Load(v);
        typename std::map<std::string, T> map;
        std::stringstream ss;
        for (auto it : node) {
            ss.str("");       // 清空字符串流
            ss << it.second;  // 将节点元素输出到字符串流中（将YAML节点转化为字符串形式）
            // 将字符串转为 T 类型后添加到 map 中
            map.insert(std::make_pair(it.first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
        }
        return map;
    }
};

// 模板偏特化，实现 map 转化为字符串
template <class T>
class LexicalCast<std::map<std::string, T>, std::string> {
   public:
    std::string operator()(const std::map<std::string, T>& v) const {
        YAML::Node node;
        for (auto& i : v) {
            node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// 模板偏特化，实现字符串转化为 map
template <class T>
class LexicalCast<std::string, std::unordered_map<std::string, T>> {
   public:
    std::unordered_map<std::string, T> operator()(const std::string& v) const {
        // 将字符串解析为YAML节点， node 相当于一个数组
        YAML::Node node = YAML::Load(v);
        typename std::unordered_map<std::string, T> unordered_map;
        std::stringstream ss;
        for (auto it : node) {
            ss.str("");       // 清空字符串流
            ss << it.second;  // 将节点元素输出到字符串流中（将YAML节点转化为字符串形式）
            // 将字符串转为 T 类型后添加到 unordered_map 中
            unordered_map.insert(
                std::make_pair(it.first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
        }
        return unordered_map;
    }
};

// 模板偏特化，实现 unordered_map 转化为字符串
template <class T>
class LexicalCast<std::unordered_map<std::string, T>, std::string> {
   public:
    std::string operator()(const std::unordered_map<std::string, T>& v) const {
        YAML::Node node;
        for (auto& i : v) {
            node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template <>
class LexicalCast<LogDefine, std::string> {
   public:
    std::string operator()(const LogDefine& val) const {
        YAML::Node node;
        node["name"] = val.name;
        node["level"] = LogLevel::ToString(val.level);
        node["formatter"] = val.formatter;
        for (const auto& appender : val.appenders) {
            YAML::Node appender_node;
            if (appender.type == 1) {
                appender_node["type"] = "FileLogAppender";
            } else if (appender.type == 2) {
                appender_node["type"] = "StdoutLogAppender";
            }
            appender_node["level"] = LogLevel::ToString(appender.level);
            appender_node["formatter"] = appender.formatter;
            appender_node["path"] = appender.path;
            appender_node["rotate_type"] = LogFile::rotateTypeToString(appender.rotateType);
            if (appender.rotateType == RotateType::SIZE && appender.maxFileSize > 0) {
                appender_node["max_size"] = appender.maxFileSize;
            }
            node["appenders"].push_back(appender_node);
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template <>
class LexicalCast<std::string, LogDefine> {
   public:
    LogDefine operator()(const std::string& val) const {
        YAML::Node node = YAML::Load(val);
        LogDefine ld;
        if (node["name"].IsDefined()) {
            ld.name = node["name"].as<std::string>();
        }
        if (node["level"].IsDefined()) {
            ld.level = LogLevel::FromString(node["level"].as<std::string>());
        }
        if (node["formatter"].IsDefined()) {
            ld.formatter = node["formatter"].as<std::string>();
        }
        if (node["appenders"].IsDefined()) {
            for (const auto& appender_node : node["appenders"]) {
                LogAppenderDefine lad;
                if (appender_node["type"].IsDefined()) {
                    if (appender_node["type"].as<std::string>() == "FileLogAppender") {
                        lad.type = 1;
                    } else if (appender_node["type"].as<std::string>() == "StdoutLogAppender") {
                        lad.type = 2;
                    }
                }
                if (appender_node["level"].IsDefined()) {
                    lad.level = LogLevel::FromString(appender_node["level"].as<std::string>());
                }
                if (appender_node["formatter"].IsDefined()) {
                    lad.formatter = appender_node["formatter"].as<std::string>();
                }
                if (appender_node["path"].IsDefined()) {
                    lad.path = appender_node["path"].as<std::string>();
                }
                if (appender_node["rotate_type"].IsDefined()) {
                    lad.rotateType = LogFile::rotateTypeFromString(
                        appender_node["rotate_type"].as<std::string>());
                }
                if (appender_node["max_size"].IsDefined()) {
                    lad.maxFileSize = appender_node["max_size"].as<uint64_t>();
                }
                ld.appenders.push_back(lad);
            }
        }
        return ld;
    }
};
}  // namespace CIM

#endif  // __CIM_CONFIG_LEXICAL_CAST_HPP__