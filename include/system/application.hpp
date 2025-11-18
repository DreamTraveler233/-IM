#ifndef __CIM_APPLICATION_HPP__
#define __CIM_APPLICATION_HPP__

#include "http/http_server.hpp"
#include "rock/rock_stream.hpp"
#include "streams/service_discovery.hpp"

namespace CIM {

/**
     * @brief 应用程序主类（单例）
     *
     * 负责：
     * - 解析并保存命令行参数
     * - 初始化服务发现与负载均衡组件
     * - 管理并启动 TcpServer 列表
     * - 创建并管理主 IO 管理器（事件循环）
     *
     * 此类提供简单的初始化和运行入口，供程序启动时调用。
     */
class Application {
   public:
    /**
         * @brief 构造函数
         *
         * 不做复杂初始化，主要用于设置单例指针（在实现中）。
         */
    Application();

    /**
         * @brief 初始化应用
         *
         * 典型职责：解析命令行、加载配置、创建 IOManager、初始化服务发现和负载均衡
         *
         * @param argc 命令行参数个数
         * @param argv 命令行参数数组
         * @return true 初始化成功
         * @return false 初始化失败
         */
    bool init(int argc, char** argv);

    /**
         * @brief 运行应用程序主循环
         *
         * 该方法通常会启动所有 server 并进入阻塞的事件循环，直到程序退出。
         *
         * @return true 正常退出
         * @return false 运行期间发生错误
         */
    bool run();

    /**
         * @brief 获取指定类型的服务器列表
         *
         * @param type 服务器类型标识（例如 "http"、"tcp" 等）
         * @param svrs 输出参数，返回对应类型的 TcpServer 智能指针列表
         * @return true 找到至少一个服务器
         * @return false 未找到对应类型的服务器
         */
    bool getServer(const std::string& type, std::vector<TcpServer::ptr>& svrs);

    /**
         * @brief 列出所有已注册的服务器
         *
         * 将内部的服务器映射拷贝到调用者提供的容器中。
         *
         * @param servers 输出参数，键为服务器类型，值为对应的服务器列表
         */
    void listAllServer(std::map<std::string, std::vector<TcpServer::ptr>>& servers);

    /**
         * @brief 获取应用的单例实例
         * @return Application* 单例指针
         */
    static Application* GetInstance();

    /**
         * @brief 获取服务发现组件（Zookeeper）
         * @return ZKServiceDiscovery::ptr
         */
    ZKServiceDiscovery::ptr getServiceDiscovery() const;

    /**
         * @brief 获取 Rock 服务发现下的负载均衡组件
         * @return RockSDLoadBalance::ptr
         */
    RockSDLoadBalance::ptr getRockSDLoadBalance() const;

   private:
    /**
         * @brief 程序主入口（内部）
         *
         * 通常由 init/run 调用，负责进一步的启动过程。
         */
    int main(int argc, char** argv);

    /**
         * @brief 以协程方式运行的逻辑（内部）
         *
         * 返回值通常表示协程执行结果或退出码。
         */
    int run_coroutine();

   private:
    /// 命令行参数个数
    int m_argc = 0;
    /// 命令行参数数组
    char** m_argv = nullptr;

    /// 按类型保存的服务器映射（key: 类型 -> value: 服务器列表）
    std::map<std::string, std::vector<TcpServer::ptr>> m_servers;
    /// 主 IO 管理器（事件循环），用于调度网络/定时等事件
    IOManager::ptr m_mainIOManager;
    /// 单例实例指针
    static Application* s_instance;

    /// 服务发现（Zookeeper）组件
    ZKServiceDiscovery::ptr m_serviceDiscovery;
    /// 基于服务发现的负载均衡组件
    RockSDLoadBalance::ptr m_rockSDLoadBalance;
};

}  // namespace CIM

#endif