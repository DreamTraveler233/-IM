/**
 * @file config_variable_base.hpp
 * @brief 配置变量基类定义文件
 * @author CIM
 *
 * 该文件定义了配置系统的基类 ConfigVariableBase，所有具体的配置变量类都需要继承自该类。
 * ConfigVariableBase 提供了配置项的基本属性（名称、描述）以及序列化/反序列化接口，
 * 是整个配置系统的核心基础类。
 */

#pragma once

#include <memory>
#include <string>

namespace CIM {
/**
     * @brief 配置变量基类
     *
     * 这是所有配置变量的基类，定义了配置项的基本接口。
     * 每个配置项都有名称和描述，并提供字符串序列化和反序列化功能。
     * 通过继承此类可以实现不同数据类型的配置变量。
     */
class ConfigVariableBase {
   public:
    /// 智能指针类型定义
    using ptr = std::shared_ptr<ConfigVariableBase>;

    /**
         * @brief 构造函数
         * @param[in] name 配置项名称
         * @param[in] description 配置项描述信息
         */
    ConfigVariableBase(const std::string& name, const std::string& description = "");

    /**
         * @brief 析构函数
         */
    virtual ~ConfigVariableBase() = default;

    /**
         * @brief 将配置项的值转换为字符串
         * @return 配置项值的字符串表示
         */
    virtual std::string toString() = 0;

    /**
         * @brief 从字符串设置配置项的值
         * @param[in] val 字符串形式的配置值
         * @return 设置成功返回true，失败返回false
         */
    virtual bool fromString(const std::string& val) = 0;

    /**
         * @brief 获取配置项值的类型名称
         * @return 类型名称字符串
         */
    virtual std::string getTypeName() const = 0;

    /**
         * @brief 获取配置项名称
         * @return 配置项名称
         */
    const std::string& getName() const;

    /**
         * @brief 获取配置项描述
         * @return 配置项描述信息
         */
    const std::string& getDescription() const;

   private:
    std::string m_name;         // 配置项名称
    std::string m_description;  // 配置项描述
};
}  // namespace CIM