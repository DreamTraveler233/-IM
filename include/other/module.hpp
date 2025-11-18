
/**
 * @file    module.hpp
 * @brief   模块系统核心接口定义，支持模块的注册、管理与生命周期控制
 * @author  DreamTraveler233
 * @date    2023-01-01
 * @note    本文件为CIM框架的模块化扩展提供基础抽象。
 */

#ifndef __CIM_MODULE_HPP__
#define __CIM_MODULE_HPP__

#include <map>
#include <unordered_map>

#include "io/lock.hpp"
#include "rock/rock_stream.hpp"
#include "base/singleton.hpp"
#include "net/stream.hpp"

namespace CIM {

/**
     * @class   Module
     * @brief   框架模块基类，定义模块生命周期与服务注册接口
     *
     * 该类为所有可插拔模块的基类，支持模块的加载、卸载、连接管理、请求处理等。
     * 通过继承本类可实现自定义业务模块。
     * @note    所有模块需唯一命名，建议通过ModuleManager统一管理。
     */
class Module {
   public:
    /**
         * @enum    Type
         * @brief   模块类型枚举
         */
    enum Type {
        MODULE = 0,  ///< 普通模块
        ROCK = 1,    ///< Rock协议模块
    };

    typedef std::shared_ptr<Module> ptr;  ///< 模块智能指针类型

    /**
         * @brief   构造函数
         * @param   name      模块名称
         * @param   version   模块版本
         * @param   filename  模块文件名
         * @param   type      模块类型，默认为MODULE
         */
    Module(const std::string& name, const std::string& version, const std::string& filename,
           uint32_t type = MODULE);

    /**
         * @brief   虚析构函数
         */
    virtual ~Module() {}

    /**
         * @brief   参数解析前回调
         * @param   argc   参数个数
         * @param   argv   参数数组
         * @note    可用于参数预处理
         */
    virtual void onBeforeArgsParse(int argc, char** argv);

    /**
         * @brief   参数解析后回调
         * @param   argc   参数个数
         * @param   argv   参数数组
         * @note    可用于参数校验
         */
    virtual void onAfterArgsParse(int argc, char** argv);

    /**
         * @brief   模块加载时回调
         * @return  加载是否成功
         */
    virtual bool onLoad();

    /**
         * @brief   模块卸载时回调
         * @return  卸载是否成功
         */
    virtual bool onUnload();

    /**
         * @brief   连接建立时回调
         * @param   stream  连接流对象
         * @return  是否处理成功
         */
    virtual bool onConnect(Stream::ptr stream);

    /**
         * @brief   连接断开时回调
         * @param   stream  连接流对象
         * @return  是否处理成功
         */
    virtual bool onDisconnect(Stream::ptr stream);

    /**
         * @brief   服务器准备就绪时回调
         * @return  是否处理成功
         */
    virtual bool onServerReady();

    /**
         * @brief   服务器启动完成时回调
         * @return  是否处理成功
         */
    virtual bool onServerUp();

    /**
         * @brief   处理请求消息
         * @param   req   请求消息
         * @param   rsp   响应消息
         * @param   stream  连接流对象
         * @return  是否处理成功
         */
    virtual bool handleRequest(Message::ptr req, Message::ptr rsp, Stream::ptr stream);

    /**
         * @brief   处理通知消息
         * @param   notify  通知消息
         * @param   stream  连接流对象
         * @return  是否处理成功
         */
    virtual bool handleNotify(Message::ptr notify, Stream::ptr stream);

    /**
         * @brief   获取模块状态字符串
         * @return  状态描述字符串
         */
    virtual std::string statusString();

    /**
         * @brief   获取模块名称
         * @return  模块名称
         */
    const std::string& getName() const;

    /**
         * @brief   获取模块版本
         * @return  模块版本
         */
    const std::string& getVersion() const;

    /**
         * @brief   获取模块文件名
         * @return  文件名
         */
    const std::string& getFilename() const;

    /**
         * @brief   获取模块唯一ID
         * @return  模块ID
         */
    const std::string& getId() const;

    /**
         * @brief   设置模块文件名
         * @param   v  文件名
         */
    void setFilename(const std::string& v);

    /**
         * @brief   获取模块类型
         * @return  类型枚举值
         */
    uint32_t getType() const;

    /**
         * @brief   注册服务信息
         * @param   server_type  服务类型
         * @param   domain       域名
         * @param   service      服务名
         * @note    用于服务发现与注册中心
         */
    void registerService(const std::string& server_type, const std::string& domain,
                         const std::string& service);

   protected:
    std::string m_name;      ///< 模块名称
    std::string m_version;   ///< 模块版本
    std::string m_filename;  ///< 模块文件名
    std::string m_id;        ///< 模块唯一ID
    uint32_t m_type;         ///< 模块类型
};

/**
     * @class   RockModule
     * @brief   Rock协议专用模块基类
     *
     * 继承自Module，专为Rock协议扩展设计，需实现Rock消息处理接口。
     * @note    需实现handleRockRequest和handleRockNotify纯虚方法。
     */
class RockModule : public Module {
   public:
    typedef std::shared_ptr<RockModule> ptr;  ///< Rock模块智能指针类型

    /**
         * @brief   构造函数
         * @param   name      模块名称
         * @param   version   模块版本
         * @param   filename  模块文件名
         */
    RockModule(const std::string& name, const std::string& version, const std::string& filename);

    /**
         * @brief   处理Rock请求
         * @param   request   Rock请求对象
         * @param   response  Rock响应对象
         * @param   stream    Rock流对象
         * @return  是否处理成功
         * @note    必须由子类实现
         */
    virtual bool handleRockRequest(RockRequest::ptr request, RockResponse::ptr response,
                                   RockStream::ptr stream) = 0;

    /**
         * @brief   处理Rock通知
         * @param   notify   Rock通知对象
         * @param   stream   Rock流对象
         * @return  是否处理成功
         * @note    必须由子类实现
         */
    virtual bool handleRockNotify(RockNotify::ptr notify, RockStream::ptr stream) = 0;

    /**
         * @brief   处理通用请求（重载）
         * @param   req   请求消息
         * @param   rsp   响应消息
         * @param   stream  连接流对象
         * @return  是否处理成功
         */
    virtual bool handleRequest(Message::ptr req, Message::ptr rsp, Stream::ptr stream);

    /**
         * @brief   处理通用通知（重载）
         * @param   notify  通知消息
         * @param   stream  连接流对象
         * @return  是否处理成功
         */
    virtual bool handleNotify(Message::ptr notify, Stream::ptr stream);
};

/**
     * @class   ModuleManager
     * @brief   模块管理器，负责模块的注册、查找、生命周期管理
     *
     * 提供线程安全的模块增删查、批量操作等功能。
     * @note    推荐通过ModuleMgr单例访问。
     */
class ModuleManager {
   public:
    typedef RWMutex RWMutexType;  ///< 读写锁类型

    /**
         * @brief   构造函数
         */
    ModuleManager();

    /**
         * @brief   添加模块
         * @param   m  模块指针
         */
    void add(Module::ptr m);

    /**
         * @brief   删除指定名称的模块
         * @param   name  模块名称
         */
    void del(const std::string& name);

    /**
         * @brief   删除所有模块
         */
    void delAll();

    /**
         * @brief   初始化所有模块
         */
    void init();

    /**
         * @brief   获取指定名称的模块
         * @param   name  模块名称
         * @return  模块指针，未找到返回nullptr
         */
    Module::ptr get(const std::string& name);

    /**
         * @brief   连接建立时通知所有模块
         * @param   stream  连接流对象
         */
    void onConnect(Stream::ptr stream);

    /**
         * @brief   连接断开时通知所有模块
         * @param   stream  连接流对象
         */
    void onDisconnect(Stream::ptr stream);

    /**
         * @brief   列出所有模块
         * @param   ms  模块指针数组
         */
    void listAll(std::vector<Module::ptr>& ms);

    /**
         * @brief   按类型列出模块
         * @param   type  模块类型
         * @param   ms    模块指针数组
         */
    void listByType(uint32_t type, std::vector<Module::ptr>& ms);

    /**
         * @brief   遍历指定类型的所有模块并执行回调
         * @param   type  模块类型
         * @param   cb    回调函数
         */
    void foreach (uint32_t type, std::function<void(Module::ptr)> cb);

   private:
    /**
         * @brief   初始化指定路径下的模块
         * @param   path  路径
         */
    void initModule(const std::string& path);

   private:
    RWMutexType m_mutex;                                     ///< 读写锁，保护模块容器
    std::unordered_map<std::string, Module::ptr> m_modules;  ///< 名称到模块的映射
    std::unordered_map<uint32_t, std::unordered_map<std::string, Module::ptr>>
        m_type2Modules;  ///< 类型到模块映射
};

/**
     * @typedef ModuleMgr
     * @brief   模块管理器单例类型
     * @note    推荐通过ModuleMgr::GetInstance()访问
     */
typedef Singleton<ModuleManager> ModuleMgr;

}  // namespace CIM

#endif