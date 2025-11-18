/**
 * @file log_file.hpp
 * @brief 文件日志类定义
 * @author CIM
 * @date 2025-10-21
 *
 * 该文件定义了文件日志类(LogFile)，用于封装单个日志文件的打开、写入、轮转、
 * 获取大小等操作。支持多种轮转类型，包括不轮转、按分钟、小时和天轮转。
 */

#ifndef __CIM_LOG_LOG_FILE_HPP__
#define __CIM_LOG_LOG_FILE_HPP__

#include <cstdint>
#include <memory>
#include <string>

namespace CIM {
/**
     * @brief 日志轮转类型枚举
     */
enum class RotateType {
    NONE,    ///< 不轮转
    MINUTE,  ///< 按分钟轮转
    HOUR,    ///< 按小时轮转
    DAY,     ///< 按天轮转
    SIZE     ///< 按文件大小轮转
};

/**
     * @brief 文件日志类
     *
     * 封装单个日志文件的打开、写入、轮转、获取大小等操作
     */
class LogFile {
   public:
    using ptr = std::shared_ptr<LogFile>;

    /**
         * @brief 构造函数
         * @param filePath 日志文件路径
         */
    LogFile(const std::string& filePath);

    /**
         * @brief 析构函数，关闭文件描述符
         */
    ~LogFile();

    /**
         * @brief 打开日志文件。
         *
         * 以追加写入方式打开指定路径的日志文件，如果文件不存在则创建。
         *
         * @return true 打开成功
         * @return false 打开失败
         */
    bool openFile();

    /**
         * @brief 写日志内容到文件。
         *
         * 将日志内容写入已打开的日志文件，如果文件未打开则写到标准输出。
         *
         * @param logMsg 日志内容字符串
         * @return size_t 实际写入的字节数
         */
    size_t writeLog(const std::string& logMsg);

    /**
         * @brief 日志文件轮转（切换）。
         *
         * 将当前日志文件重命名为新文件名，并打开一个新的日志文件，实现日志文件的无缝切换。
         *
         * @param newFilePath 新日志文件路径
         */
    void rotate(const std::string& newFilePath);

    /**
         * @brief 获取当前日志文件的大小（字节数）。
         *
         * 使用 lseek64 将文件指针移动到文件末尾，并返回文件总字节数。
         *
         * @return int64_t 文件大小（字节数）
         */
    int64_t getFileSize() const;

    /**
         * @brief 获取当前日志文件路径。
         *
         * @return std::string 日志文件路径
         */
    const std::string& getFilePath() const;

    /**
         * @brief 设置日志轮转类型。
         *
         * @param type 日志轮转类型
         */
    void setRotateType(RotateType type);

    /**
         * @brief 获取日志轮转类型。
         *
         * @return RotateType 当前日志轮转类型
         */
    RotateType getRotateType() const;

    /**
         * @brief 设置触发大小轮转的阈值（字节）。
         *
         * @param size 阈值，0 表示关闭大小轮转
         */
    void setMaxFileSize(uint64_t size);

    /**
         * @brief 获取大小轮转阈值（字节）。
         *
         * @return uint64_t 大小阈值
         */
    uint64_t getMaxFileSize() const;

    /**
         * @brief 从字符串转换轮转类型
         * @param rotateType 轮转类型字符串
         * @return RotateType 轮转类型枚举值
         */
    static RotateType rotateTypeFromString(const std::string& rotateType);

    /**
         * @brief 将轮转类型转换为字符串
         * @param rotateType 轮转类型枚举值
         * @return std::string 轮转类型字符串
         */
    static std::string rotateTypeToString(RotateType rotateType);

   private:
    int m_fd;                 ///< 文件描述符
    std::string m_filePath;   ///< 日志文件路径
    RotateType m_rotateType;  ///< 日志轮转类型
    uint64_t m_maxFileSize;   ///< 大小轮转阈值
};
}  // namespace CIM

#endif  // __CIM_LOG_LOG_FILE_HPP__