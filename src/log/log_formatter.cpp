#include "log/log_formatter.hpp"

#include <functional>
#include <sstream>

#include "log/log_event.hpp"
#include "log/logger.hpp"

namespace CIM {
LogFormatter::LogFormatter(const std::string& pattern) : m_pattern(pattern), m_isError(false) {
    CIM_ASSERT(!pattern.empty());
    init();
}

std::string LogFormatter::format(std::shared_ptr<LogEvent> event) {
    CIM_ASSERT(event);
    std::stringstream ss;
    for (auto& i : m_items) {
        i->format(ss, event);
    }
    return ss.str();
}

bool LogFormatter::isError() const {
    return m_isError;
}
const std::string& LogFormatter::getPattern() const {
    return m_pattern;
}
/**
     * @brief 初始化解析日志格式模式
     *
     * 该函数解析传入的格式模式字符串，将其分解为普通字符串和格式化项，
     * 并根据格式化项创建对应的格式化对象。
     *
     * 解析规则：
     * 1. 普通字符直接作为字符串处理
     * 2. %后跟字母表示格式化项
     * 3. %%表示转义的%字符
     */
void LogFormatter::init() {
    // 存储解析后的格式项，每个元组包含：字符串内容、格式参数、类型(0-普通字符串，1-格式化项)
    std::vector<std::map<std::string, int>> vec;
    // 存储普通字符串
    std::string nstr;

    // 遍历格式模式字符串，逐字符解析
    for (size_t i = 0; i < m_pattern.size(); ++i) {
        // 如果当前字符不是%，则作为普通字符串处理
        if (m_pattern[i] != '%') {
            nstr.append(1, m_pattern[i]);
            continue;
        }

        // 处理连续的两个%%，表示转义的%字符
        if ((i + 1) < m_pattern.size() && m_pattern[i + 1] == '%') {
            nstr.append(1, '%');
            continue;
        }

        // 开始解析格式化项
        size_t n = i + 1;
        std::string str;  // 格式项名称

        // 查找格式化项名称（字母）
        while (n < m_pattern.size() && isalpha(m_pattern[n])) {
            str.append(1, m_pattern[n]);
            ++n;
        }

        // 如果没有找到格式化项名称，则是错误
        if (str.empty()) {
            std::cout << "pattern parse error: " << m_pattern << " - " << m_pattern.substr(i)
                      << std::endl;
            m_isError = true;
            vec.push_back({{"<<pattern_error>>", 0}});
            continue;
        }

        // 如果存在普通字符串，先将其加入结果列表
        if (!nstr.empty()) {
            vec.push_back({{nstr, 0}});
            nstr.clear();
        }

        // 将解析到的格式项加入结果列表
        vec.push_back({{str, 1}});
        i = n - 1;
    }

    // 处理最后剩余的普通字符串
    if (!nstr.empty()) {
        vec.push_back({{nstr, 0}});
    }

    // 定义格式项创建函数映射表
    static std::map<std::string, std::function<FormatItem::ptr()>> s_format_items = {
#define XX(str, C)                                      \
    {                                                   \
        #str, []() { return FormatItem::ptr(new C()); } \
    }
        XX(m, MessageFormatItem),     // %m -- 消息体
        XX(p, LevelFormatItem),       // %p -- level
        XX(r, ElapseFormatItem),      // %r -- 启动后的时间
        XX(c, NameFormatItem),        // %c -- 日志名称
        XX(t, ThreadIdFormatItem),    // %t -- 线程ID
        XX(N, ThreadNameFormatItem),  // %N -- 线程名称
        XX(n, NewLineFormatItem),     // %n -- 回车换行
        XX(d, DateTimeFormatItem),    // %d -- 时间
        XX(f, FileNameFormatItem),    // %f -- 文件名
        XX(l, LineFormatItem),        // %l -- 行号
        XX(T, TabFormatItem),         // %T -- Tab
        XX(F, FiberIdFormatItem),     // %F -- 协程ID
#undef XX
    };

    // 根据解析结果创建对应的格式化项对象
    for (auto& i : vec) {
        auto it_map = i.begin();
        std::string content = it_map->first;
        int type = it_map->second;
        if (type == 0) {
            // 普通字符串，创建字符串格式化项
            m_items.push_back(FormatItem::ptr(new StringFormatItem(content)));
        } else {
            // 格式化项，查找对应的创建函数
            auto it = s_format_items.find(content);
            if (it == s_format_items.end()) {
                // 未找到对应的格式化项，创建错误提示项
                m_items.push_back(
                    FormatItem::ptr(new StringFormatItem("<<error_format %" + content + ">>")));
                m_isError = true;
            } else {
                // 创建对应的格式化项对象，不传递任何参数
                m_items.push_back(it->second());
            }
        }
    }
}

void MessageFormatItem::format(std::ostream& os, std::shared_ptr<LogEvent> event) {
    os << event->getMessage();
}

void LevelFormatItem::format(std::ostream& os, std::shared_ptr<LogEvent> event) {
    os << LogLevel::ToString(event->getLevel());
}

void ElapseFormatItem::format(std::ostream& os, std::shared_ptr<LogEvent> event) {
    os << event->getElapse();
}

void NameFormatItem::format(std::ostream& os, std::shared_ptr<LogEvent> event) {
    os << event->getLogger()->getName();
}

void ThreadIdFormatItem::format(std::ostream& os, std::shared_ptr<LogEvent> event) {
    os << event->getThreadId();
}

DateTimeFormatItem::DateTimeFormatItem() : m_format("%Y-%m-%d %H:%M:%S") {}

void DateTimeFormatItem::format(std::ostream& os, std::shared_ptr<LogEvent> event) {
    struct tm tm;
    time_t time = event->getTime();
    localtime_r(&time, &tm);
    char buf[64];
    strftime(buf, sizeof(buf), m_format.c_str(), &tm);
    os << buf;
}

void FileNameFormatItem::format(std::ostream& os, std::shared_ptr<LogEvent> event) {
    os << event->getRelativeFileName();
}

void LineFormatItem::format(std::ostream& os, std::shared_ptr<LogEvent> event) {
    os << event->getLine();
}

void NewLineFormatItem::format(std::ostream& os, std::shared_ptr<LogEvent> event) {
    os << std::endl;
}

void TabFormatItem::format(std::ostream& os, std::shared_ptr<LogEvent> event) {
    os << "\t";
}

void FiberIdFormatItem::format(std::ostream& os, std::shared_ptr<LogEvent> event) {
    os << event->getCoroutineId();
}

StringFormatItem::StringFormatItem(const std::string& fmt) : m_string(fmt) {}
void StringFormatItem::format(std::ostream& os, std::shared_ptr<LogEvent> event) {
    os << m_string;
}

void ThreadNameFormatItem::format(std::ostream& os, std::shared_ptr<LogEvent> event) {
    os << event->getThreadName();
}
}  // namespace CIM