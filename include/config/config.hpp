/**
 * @file config.hpp
 * @brief 配置管理器定义文件
 * @author CIM
 *
 * 该文件定义了配置系统的核心管理类 Config，负责配置项的注册、查找、加载和管理。
 * Config 类提供了全局的配置管理功能，支持从 YAML 文件加载配置，并提供线程安全的
 * 配置项访问接口。通过模板方法支持任意类型的配置项管理。
 */

#pragma once

#include "config_variable_base.hpp"
#include "config_var.hpp"
#include "io/lock.hpp"
#include "base/macro.hpp"
#include "yaml-cpp/yaml.h"

namespace CIM {
/**
     * @brief 配置管理器类
     *
     * Config 是配置系统的核心管理类，提供了全局的配置项管理功能。
     * 主要功能包括：
     * 1. 配置项的注册和查找
     * 2. 从 YAML 文件加载配置
     * 3. 线程安全的配置访问
     * 4. 配置项遍历功能
     *
     * 使用单例模式管理所有配置项，通过静态方法提供全局访问接口。
     */
class Config {
   public:
    /// 配置项映射类型定义，键为配置项名称，值为配置项基类指针
    using ConfigVarMap = std::map<std::string, ConfigVariableBase::ptr>;

    /// 读写锁类型定义
    using RWMutexType = RWMutex;

    /**
         * @brief 查找或创建配置项
         * @tparam T 配置项的数据类型
         * @param name 配置项名称
         * @param default_value 配置项默认值
         * @param description 配置项描述信息
         * @return 返回配置项的智能指针
         *
         * 1、根据名称查找已存在的配置项，如果找到且类型匹配则返回
         * 2、如果找到但类型不匹配则记录错误日志
         * 3、如果未找到且名称合法，则创建新的配置项并返回
         * 4、如果名称不合法则抛出异常
         */
    template <class T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name, const T& default_value,
                                             const std::string& description = "") {
        CIM_ASSERT(!name.empty());
        RWMutexType::WriteLock lock(GetMutex());
        // 根据配置项名称在存储容器中查找
        auto it = GetDatas().find(name);
        // 如果找到了同名配置项
        if (it != GetDatas().end()) {
            // 尝试将找到的配置项基类指针转换为指定类型T的配置项指针
            // dynamic_pointer_cast会在运行时检查类型兼容性，如果类型不匹配会返回nullptr
            auto tmp = std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
            // 如果类型转换成功，说明找到了同名且同类型的配置项
            if (tmp) {
                CIM_LOG_INFO(CIM_LOG_ROOT()) << "Lookup name = " << name << " exists";
                return tmp;
            }
            // 如果类型转换失败，说明虽然找到了同名配置项，但其实际类型与请求的类型T不匹配
            else {
                CIM_LOG_ERROR(CIM_LOG_ROOT())
                    << "Lookup name = " << name << " exists but type not " << typeid(T).name()
                    << " real_type = " << it->second->getTypeName() << " "
                    << "value = " << it->second->toString();
                // 抛出异常
                throw std::runtime_error(
                    "Config variable '" + name + "' exists but type mismatch. Requested: " +
                    std::string(typeid(T).name()) + ", Actual: " + it->second->getTypeName());
            }
        }

        // 如果没有到了同名配置项，则创建该配置项
        // 检查配置项名称是否合法，只能包含字母、数字、下划线和点号
        if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._") != std::string::npos) {
            CIM_LOG_ERROR(CIM_LOG_ROOT()) << "lookup name invalid name=" << name;
            throw std::invalid_argument(name);  // 抛出异常
        }

        // 创建新的配置项
        typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, default_value, description));
        GetDatas()[name] = v;
        return v;
    }

    /**
         * @brief 根据名称查找配置项
         * @tparam T 配置项的数据类型
         * @param name 配置项名称
         * @return 返回找到的配置项指针，如果未找到则返回nullptr
         */
    template <class T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name) {
        CIM_ASSERT(!name.empty());
        RWMutexType::ReadLock lock(GetMutex());
        auto it = GetDatas().find(name);
        if (it == GetDatas().end()) {
            return nullptr;
        }
        // 尝试将配置项转换为指定数据类型
        return std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
    }

    /**
         * @brief 根据名称查找配置项（基类版本）
         * @param name 配置项名称
         * @return 找到的配置项指针，如果未找到则返回nullptr
         */
    static ConfigVariableBase::ptr LookupBase(const std::string& name);

    /**
         * @brief 从指定目录加载配置文件
         * @param path 配置文件目录路径
         * @param force 是否强制加载，即使文件未修改也重新加载
         */
    static void LoadFromConfigDir(const std::string& path, bool force = false);

    /**
         * @brief 从YAML配置文件中加载配置项
         * @param root YAML配置文件的根节点
         */
    static void LoadFromYaml(const YAML::Node& root);

    /**
         * @brief 遍历配置模块里面所有配置项
         * @param[in] cb 配置项回调函数
         */
    static void Visit(std::function<void(ConfigVariableBase::ptr)> cb);

   private:
    /**
         * 通过静态函数和静态局部变量来解决静态初始化顺序问题，函数首次调用时会初始化这个静态变量，
         * 之后的调用都会返回同一个实例的引用，这样防止了当该静态变量没有初始化就被使用
         * @return 配置项存储容器的引用
         */
    static ConfigVarMap& GetDatas() {
        static ConfigVarMap m_datas;  // 配置项存储
        return m_datas;
    }

    /**
         * @return 读写锁的引用
         */
    static RWMutexType& GetMutex() {
        static RWMutexType m_mutex;  // 读写锁
        return m_mutex;
    }
};
}  // namespace CIM