/**
 * @file config_var.hpp
 * @brief 配置变量模板类定义文件
 * @author CIM
 *
 * 该文件定义了配置系统中的核心模板类 ConfigVar，用于存储和管理各种类型的配置项。
 * ConfigVar 继承自 ConfigVariableBase，提供了类型安全的配置项管理功能，
 * 支持配置值的序列化/反序列化、变更监听回调等特性。
 */

#pragma once

#include "io/lock.hpp"
#include "lexical_cast.hpp"
#include "base/macro.hpp"

namespace CIM {
/**
     * @brief 配置变量模板类
     * @tparam T 配置项的数据类型
     * @tparam fromStr 从字符串转换为T类型的转换器，默认使用LexicalCast
     * @tparam toStr 从T类型转换为字符串的转换器，默认使用LexicalCast
     *
     * ConfigVar 是配置系统的核心类，它封装了特定类型的配置项。
     * 通过模板参数可以支持任意数据类型，并提供配置值的自动序列化和反序列化功能。
     * 同时支持配置变更时的回调通知机制。
     */
template <class T, class fromStr = LexicalCast<std::string, T>,
          class toStr = LexicalCast<T, std::string>>
class ConfigVar : public ConfigVariableBase {
   public:
    /// 智能指针类型定义
    using ptr = std::shared_ptr<ConfigVar>;

    /// 配置变更回调函数类型，当配置值发生变更时被调用
    using ConfigChangeCb = std::function<void(const T& old_value, const T& new_value)>;

    /// 读写锁类型定义
    using RWMutexType = RWMutex;

    /**
         * @brief 构造函数
         * @param[in] name 配置项名称
         * @param[in] default_value 配置项默认值
         * @param[in] description 配置项描述信息
         */
    ConfigVar(const std::string& name, const T& default_value, const std::string& description)
        : ConfigVariableBase(name, description), m_val(default_value) {
        CIM_ASSERT(!name.empty());
    }

    /**
         * @brief 将配置项的值转换为字符串表示
         * @return std::string 配置值的字符串表示，转换失败时返回空字符串
         *
         * 1、使用toStr()获取转换函数，将成员变量m_val转为字符串
         * 2、若转换过程出现异常，则记录错误日志并返回空字符串
         * 3、主要用于配置项的序列化输出
         */
    std::string toString() override {
        try {
            RWMutexType::ReadLock lock(m_mutex);
            return toStr()(m_val);
        } catch (std::exception& e) {
            CIM_LOG_ERROR(CIM_LOG_ROOT()) << "ConfigVar::toString exception" << e.what()
                                          << " convert: " << typeid(m_val).name() << " to string";
        }
        return "";
    }

    /**
         * @brief 将字符串转换为配置项的值
         * @param[in] val 待转换的字符串
         * @return 转换成功返回true，失败返回false
         *
         * 1、使用fromStr()将输入字符串转换为目标类型值
         * 2、调用setValue()设置转换后的值
         * 3、捕获转换异常并记录错误日志
         * 4、成功转换返回true，失败返回false
         */
    bool fromString(const std::string& val) override {
        CIM_ASSERT(!val.empty());
        try {
            setValue(fromStr()(val));
            return true;
        } catch (std::exception& e) {
            CIM_LOG_ERROR(CIM_LOG_ROOT())
                << "ConfigVar::fromString exception" << e.what() << " convert: string to"
                << typeid(m_val).name() << " - " << val;
        }
        return false;
    }

    /**
         * @brief 获取配置项值的类型名称
         * @return 返回T类型的typeid名称
         */
    std::string getTypeName() const override { return typeid(T).name(); }

    /**
         * @brief 设置配置项的值
         * @param[in] v 新的配置值
         *
         * 如果新值与当前值相同则不进行任何操作。
         * 否则更新配置值，并触发所有注册的变更回调函数。
         * 回调函数在无锁状态下执行，避免潜在的死锁问题。
         */
    void setValue(const T& v) {
        T old_value;
        std::map<uint64_t, ConfigChangeCb> cbs;
        {
            RWMutexType::WriteLock lock(m_mutex);

            if (v == m_val)  // 值没有改变
            {
                return;
            }
            old_value = m_val;
            m_val = v;
            cbs = m_cbs;
        }

        // 执行回调函数时不持有锁，防止回调函数中可能的死锁
        for (auto& i : cbs) {
            i.second(old_value, v);
        }
    }

    /**
         * @brief 获取配置项的值
         * @return 当前配置项的值
         */
    T getValue() const {
        RWMutexType::ReadLock lock(m_mutex);
        return m_val;
    }

    /**
         * @brief 添加配置变更监听器
         * @param[in] cb 配置变更回调函数
         * @return 监听器ID，可用于删除监听器
         */
    uint64_t addListener(const ConfigChangeCb& cb) {
        CIM_ASSERT(cb);
        static uint64_t func_id = 0;
        RWMutexType::WriteLock lock(m_mutex);
        ++func_id;
        m_cbs[func_id] = cb;
        return func_id;
    }

    /**
         * @brief 删除指定的配置变更监听器
         * @param[in] key 监听器ID
         */
    void delListener(uint64_t key) {
        CIM_ASSERT(key > 0);
        RWMutexType::WriteLock lock(m_mutex);
        if (m_cbs.find(key) != m_cbs.end()) {
            CIM_LOG_INFO(CIM_LOG_ROOT())
                << "Removing listener for config variable: " << getName() << " with key: " << key;
            m_cbs.erase(key);
        } else {
            CIM_LOG_WARN(CIM_LOG_ROOT())
                << "Trying to remove non-existent listener for config variable: " << getName()
                << " with key: " << key;
        }
    }

    /**
         * @brief 清除所有配置变更监听器
         */
    void clearListener() {
        RWMutexType::WriteLock lock(m_mutex);
        m_cbs.clear();
    }

    /**
         * @brief 获取指定的配置变更监听器
         * @param[in] key 监听器ID
         * @return 对应的回调函数，如果不存在则返回nullptr
         */
    ConfigChangeCb getListener(uint64_t key) {
        CIM_ASSERT(key > 0);
        RWMutexType::ReadLock lock(m_mutex);
        auto it = m_cbs.find(key);
        return it == m_cbs.end() ? nullptr : it->second;
    }

   private:
    T m_val;                                   // 配置值
    std::map<uint64_t, ConfigChangeCb> m_cbs;  // 配置改变时的回调函数
    RWMutexType m_mutex;                       // 读写锁
};
}  // namespace CIM