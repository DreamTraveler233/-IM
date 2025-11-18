/**
 * @file env.hpp
 * @brief 环境变量管理类
 * @author xxx
 * @date 2025-10-21
 *
 * 该文件定义了环境变量管理类Env及其单例模式EnvMgr，用于管理系统环境变量、
 * 命令行参数以及程序相关信息，提供了获取程序路径、工作目录、配置路径等功能。
 */

#pragma once

#include <map>
#include <vector>

#include "io/lock.hpp"
#include "base/singleton.hpp"
#include "io/thread.hpp"

namespace CIM {
/**
     * @brief 环境变量管理类
     *
     * 提供对系统环境变量和命令行参数的统一管理，支持设置、获取、删除环境变量，
     * 并可以处理程序相关信息如可执行文件路径、当前工作目录等。
     */
class Env {
   public:
    /// 读写锁类型定义
    using RWMutexType = RWMutex;

    /**
         * @brief 初始化环境管理器
         * @param[in] argc 参数个数
         * @param[in] argv 参数列表
         * @return 是否初始化成功
         */
    bool init(int argc, char** argv);

    /**
         * @brief 添加键值对参数
         * @param[in] key 键
         * @param[in] val 值
         */
    void add(const std::string& key, const std::string& val);

    /**
         * @brief 判断是否存在指定键
         * @param[in] key 键
         * @return 是否存在
         */
    bool has(const std::string& key);

    /**
         * @brief 删除指定键
         * @param[in] key 键
         */
    void del(const std::string& key);

    /**
         * @brief 获取指定键的值
         * @param[in] key 键
         * @param[in] default_value 默认值
         * @return 键对应的值，不存在则返回默认值
         */
    std::string get(const std::string& key, const std::string& default_value = "");

    /**
         * @brief 添加帮助信息
         * @param[in] key 键
         * @param[in] desc 描述信息
         */
    void addHelp(const std::string& key, const std::string& desc);

    /**
         * @brief 移除帮助信息
         * @param[in] key 键
         */
    void removeHelp(const std::string& key);

    /**
         * @brief 打印帮助信息
         */
    void printHelp();

    /**
         * @brief 获取可执行文件路径
         * @return 可执行文件路径
         */
    const std::string& getExe() const { return m_exe; }

    /**
         * @brief 获取当前工作目录
         * @return 当前工作目录
         */
    const std::string& getCwd() const { return m_cwd; }

    /**
         * @brief 设置环境变量
         * @param[in] key 键
         * @param[in] val 值
         * @return 是否设置成功
         */
    bool setEnv(const std::string& key, const std::string& val);

    /**
         * @brief 获取环境变量
         * @param[in] key 键
         * @param[in] default_value 默认值
         * @return 环境变量值，不存在则返回默认值
         */
    std::string getEnv(const std::string& key, const std::string& default_value = "");

    /**
         * @brief 获取可执行文件绝对路径
         * @param[in] path 路径
         * @return 绝对路径
         */
    std::string getAbsolutePath(const std::string& path) const;

    /**
         * @brief 获取可执行文件绝对工作路径
         * @param[in] path 路径
         * @return 绝对工作路径
         */
    std::string getAbsoluteWorkPath(const std::string& path) const;

    /**
         * @brief 获取配置路径
         * @return 配置路径
         */
    std::string getConfigPath();

   private:
    RWMutexType m_mutex;                                       /// 读写互斥锁
    std::map<std::string, std::string> m_args;                 /// 参数映射表
    std::vector<std::pair<std::string, std::string>> m_helps;  /// 帮助信息列表
    std::string m_program;                                     /// 程序名
    std::string m_exe;                                         /// 可执行文件的绝对路径
    std::string m_cwd;                                         /// 可执行文件绝对工作目录
};

/// 环境管理器单例
using EnvMgr = Singleton<Env>;

}  // namespace CIM