/**
 * @file tcp_server.hpp
 * @brief TCP服务器封装
 * @author your_name
 *
 * @details 该文件实现了TCP服务器的封装，提供了完整的TCP服务器功能，
 * 包括地址绑定、连接监听、客户端连接处理等。基于CIM框架的协程和IO管理器实现，
 * 支持高并发的网络连接处理。
 *
 * 主要特性:
 * - 支持多地址绑定
 * - 基于协程的异步IO处理
 * - SSL/TLS加密连接支持
 * - 可配置的超时时间和keepalive机制
 * - 灵活的工作线程模型
 */

#pragma once

#include <functional>
#include <memory>

#include "address.hpp"
#include "config/config.hpp"
#include "io/iomanager.hpp"
#include "base/noncopyable.hpp"
#include "socket.hpp"

namespace CIM {
/**
     * @brief TCP服务器配置结构体
     * @details 用于存储TCP服务器的各种配置参数
     */
struct TcpServerConf {
    typedef std::shared_ptr<TcpServerConf> ptr;

    std::vector<std::string> address;         /// 监听地址列表
    int keepalive = 0;                        /// keepalive选项
    int timeout = 1000 * 2 * 60;              /// 超时时间(毫秒)，默认4分钟
    int ssl = 0;                              /// 是否启用SSL
    std::string id;                           /// 服务器唯一标识
    std::string type = "http";                /// 服务器类型，如"http", "ws", "rock"
    std::string name;                         /// 服务器名称
    std::string cert_file;                    /// 证书文件路径
    std::string key_file;                     /// 私钥文件路径
    std::string accept_worker;                /// 接受连接的工作者名称
    std::string io_worker;                    /// IO操作的工作者名称
    std::string process_worker;               /// 处理业务的工作者名称
    std::map<std::string, std::string> args;  /// 其他参数

    /**
         * @brief 检查配置是否有效
         * @return true表示配置有效
         */
    bool isValid() const { return !address.empty(); }

    /**
         * @brief 比较两个配置是否相等
         * @param[in] oth 待比较的配置
         * @return true表示相等
         */
    bool operator==(const TcpServerConf& oth) const {
        return address == oth.address && keepalive == oth.keepalive && timeout == oth.timeout &&
               name == oth.name && ssl == oth.ssl && cert_file == oth.cert_file &&
               key_file == oth.key_file && accept_worker == oth.accept_worker &&
               io_worker == oth.io_worker && process_worker == oth.process_worker &&
               args == oth.args && id == oth.id && type == oth.type;
    }
};

template <>
class LexicalCast<std::string, TcpServerConf> {
   public:
    TcpServerConf operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        TcpServerConf conf;
        conf.id = node["id"].as<std::string>(conf.id);
        conf.type = node["type"].as<std::string>(conf.type);
        conf.keepalive = node["keepalive"].as<int>(conf.keepalive);
        conf.timeout = node["timeout"].as<int>(conf.timeout);
        conf.name = node["name"].as<std::string>(conf.name);
        conf.ssl = node["ssl"].as<int>(conf.ssl);
        conf.cert_file = node["cert_file"].as<std::string>(conf.cert_file);
        conf.key_file = node["key_file"].as<std::string>(conf.key_file);
        conf.accept_worker = node["accept_worker"].as<std::string>();
        conf.io_worker = node["io_worker"].as<std::string>();
        conf.process_worker = node["process_worker"].as<std::string>();
        conf.args = LexicalCast<std::string, std::map<std::string, std::string>>()(
            node["args"].as<std::string>(""));
        if (node["address"].IsDefined()) {
            for (size_t i = 0; i < node["address"].size(); ++i) {
                conf.address.push_back(node["address"][i].as<std::string>());
            }
        }
        return conf;
    }
};

template <>
class LexicalCast<TcpServerConf, std::string> {
   public:
    std::string operator()(const TcpServerConf& conf) {
        YAML::Node node;
        node["id"] = conf.id;
        node["type"] = conf.type;
        node["name"] = conf.name;
        node["keepalive"] = conf.keepalive;
        node["timeout"] = conf.timeout;
        node["ssl"] = conf.ssl;
        node["cert_file"] = conf.cert_file;
        node["key_file"] = conf.key_file;
        node["accept_worker"] = conf.accept_worker;
        node["io_worker"] = conf.io_worker;
        node["process_worker"] = conf.process_worker;
        node["args"] =
            YAML::Load(LexicalCast<std::map<std::string, std::string>, std::string>()(conf.args));
        for (auto& i : conf.address) {
            node["address"].push_back(i);
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

/**
     * @brief TCP服务器封装类
     * @details 提供TCP服务器的基本功能，包括监听端口、接受连接、处理客户端请求等。
     * 基于CIM框架的协程和IO管理器实现，能够处理大量并发连接。
     *
     * 核心组件:
     * - Socket管理: 管理监听套接字和客户端连接套接字
     * - 协程调度: 使用IOManager进行协程调度
     * - 连接处理: 接受新连接并分发给工作协程处理
     * - SSL支持: 可选的SSL/TLS加密连接支持
     *
     * 工作流程:
     * 1. 创建TcpServer实例
     * 2. 绑定监听地址(bind)
     * 3. 启动服务器(start)
     * 4. 接受客户端连接并分发到工作协程
     * 5. 子类重写handleClient处理具体业务逻辑
     */
class TcpServer : public std::enable_shared_from_this<TcpServer>, Noncopyable {
   public:
    typedef std::shared_ptr<TcpServer> ptr;

    /**
         * @brief 构造函数
         * @param[in] worker socket客户端工作的协程调度器
         * @param[in] io_worker IO操作的协程调度器
         * @param[in] accept_worker 服务器socket执行接收socket连接的协程调度器
         */
    TcpServer(IOManager* worker = IOManager::GetThis(), IOManager* io_worker = IOManager::GetThis(),
              IOManager* accept_worker = IOManager::GetThis());

    /**
         * @brief 析构函数，关闭所有套接字
         */
    virtual ~TcpServer();

    /**
         * @brief 绑定单个地址
         * @param[in] addr 需要绑定的地址
         * @param[in] ssl 是否启用SSL
         * @return 返回是否绑定成功
         */
    virtual bool bind(Address::ptr addr, bool ssl = false);

    /**
         * @brief 绑定一组地址
         * @param[in] addrs 需要绑定的地址数组
         * @param[out] fails 绑定失败的地址
         * @param[in] ssl 是否启用SSL
         * @return 是否绑定成功
         */
    virtual bool bind(const std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& fails,
                      bool ssl = false);

    /**
         * @brief 加载SSL证书
         * @param[in] cert_file 证书文件路径
         * @param[in] key_file 私钥文件路径
         * @return 是否加载成功
         */
    bool loadCertificates(const std::string& cert_file, const std::string& key_file);

    /**
         * @brief 启动 TcpServer
         * @pre 需要bind成功后执行
         * @return 是否启动成功
         */
    virtual bool start();

    /**
         * @brief 停止服务
         */
    virtual void stop();

    /**
         * @brief 返回读取超时时间(毫秒)
         * @return 读取超时时间
         */
    uint64_t getRecvTimeout() const;

    /**
         * @brief 返回服务器名称
         * @return 服务器名称
         */
    std::string getName() const;

    /**
         * @brief 设置读取超时时间(毫秒)
         * @param[in] v 超时时间
         */
    void setRecvTimeout(uint64_t v);

    /**
         * @brief 设置服务器名称
         * @param[in] v 服务器名称
         */
    virtual void setName(const std::string& v);

    /**
         * @brief 是否停止
         * @return true表示服务器已停止
         */
    bool isRun() const;

    /**
         * @brief 获取服务器配置
         * @return 服务器配置
         */
    TcpServerConf::ptr getConf() const;

    /**
         * @brief 设置服务器配置
         * @param[in] v 服务器配置
         */
    void setConf(TcpServerConf::ptr v);

    /**
         * @brief 设置服务器配置
         * @param[in] v 服务器配置
         */
    void setConf(const TcpServerConf& v);

    /**
         * @brief 转换为字符串表示
         * @param[in] prefix 前缀字符串
         * @return 字符串表示
         */
    virtual std::string toString(const std::string& prefix = "");

    /**
         * @brief 获取监听套接字列表
         * @return 套接字列表
         */
    std::vector<Socket::ptr> getSocks() const;

   protected:
    /**
         * @brief 连接服务器后处理的业务逻辑
         * @param[in] client 客户端Socket
         * @note 子类可以重写此方法来实现具体的业务逻辑
         */
    virtual void handleClient(Socket::ptr client);

    /**
         * @brief 开始接受连接
         * @param[in] sock 监听Socket
         */
    virtual void startAccept(Socket::ptr sock);

   protected:
    std::vector<Socket::ptr> m_socks;  /// 监听Socket数组
    IOManager* m_worker;               /// 新连接的Socket工作的调度器
    IOManager* m_ioWorker;             /// IO操作的调度器
    IOManager* m_acceptWorker;         /// 服务器创建新连接的调度器
    uint64_t m_recvTimeout;            /// 接收超时时间(毫秒)
    std::string m_name;                /// 服务器名称
    std::string m_type = "tcp";        /// 服务器类型
    bool m_isRun;                      /// 服务是否运行
    bool m_ssl = false;                /// 是否启用SSL
    TcpServerConf::ptr m_conf;         /// 服务器配置
};
}  // namespace CIM