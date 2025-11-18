#pragma once

#include <stdint.h>

#include <ctime>
#include <string>

namespace CIM {
class TimeUtil {
   public:
    /**
         * @brief 获取当前 UTC 时间（毫秒级）。
         *
         * @return uint64_t 当前 UTC 时间，单位为毫秒
         */
    static uint64_t NowToMS();

    /**
         * @brief 获取当前 UTC 时间（微秒级）。
         *
         * @return uint64_t 当前 UTC 时间，单位为微秒
         */
    static uint64_t NowToUS();

    /**
         * @brief 获取当前 UTC 时间（秒级）。
         *
         * @return uint64_t 当前 UTC 时间，单位为秒
         */
    static uint64_t NowToS();

    /**
         * @brief 获取当前本地时间的年月日时分秒，并返回当前时间戳（秒）。
         *
         * @param[out] year   年
         * @param[out] month  月
         * @param[out] day    日
         * @param[out] hour   时
         * @param[out] minute 分
         * @param[out] second 秒
         * @return uint64_t 当前时间戳（秒）
         */
    static uint64_t Now(int& year, int& month, int& day, int& hour, int& minute, int& second);

    /**
         * @brief 获取当前时间的 ISO 格式字符串（yyyy-MM-ddTHH:mm:ss）。
         *
         * @return std::string 当前时间的 ISO 格式字符串
         */
    static std::string NowToString();

    /**
         * @brief 将时间戳转换为指定格式的时间字符串
         *
         * @param timestamp 时间戳（秒）
         * @param format 格式字符串，如 "%Y-%m-%d %H:%M:%S"
         * @return std::string 格式化后的时间字符串
         */
    static std::string TimeToStr(time_t timestamp = time(0),
                                 const std::string& format = "%Y-%m-%d %H:%M:%S");

    static time_t StrToTime(const char* str, const char* format = "%Y-%m-%d %H:%M:%S");
};
}  // namespace CIM