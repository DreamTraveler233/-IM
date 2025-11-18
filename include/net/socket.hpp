/**
 * @file socket.hpp
 * @brief Socket封装头文件
 * @author CIM
 *
 * 该文件定义了Socket和SSLSocket两个核心网络类，用于处理各种类型的网络通信：
 * 1. 支持TCP和UDP协议
 * 2. 支持IPv4、IPv6和Unix域套接字
 * 3. 提供SSL/TLS加密通信支持
 * 4. 集成协程友好的异步I/O操作
 *
 * 主要特性：
 * - 面向对象设计：通过Socket类封装底层socket操作
 * - 工厂模式：提供多种静态方法创建不同类型的Socket实例
 * - 智能指针管理：使用shared_ptr进行内存管理，避免内存泄漏
 * - 异常安全：在网络操作失败时返回适当的错误码而非抛出异常
 * - 超时控制：支持发送和接收操作的超时设置
 * - 协程支持：与CIM框架的协程系统无缝集成
 * - SSL支持：SSLSocket子类提供安全的网络通信
 */

#pragma once

#include <netinet/tcp.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <memory>

#include "address.hpp"
#include "base/noncopyable.hpp"

namespace CIM {
/**
     * @brief Socket封装类
     *
     * 提供对底层socket的高级封装，支持TCP、UDP协议以及IPv4、IPv6和Unix域套接字。
     * 同时还提供了SSL/TLS加密通信的支持。
     */
class Socket : public std::enable_shared_from_this<Socket>, Noncopyable {
   public:
    typedef std::shared_ptr<Socket> ptr;
    typedef std::weak_ptr<Socket> weak_ptr;

    /**
         * @brief Socket类型枚举
         */
    enum Type {
        /// TCP类型
        TCP = SOCK_STREAM,
        /// UDP类型
        UDP = SOCK_DGRAM
    };

    /**
         * @brief Socket协议簇枚举
         */
    enum Family {
        /// IPv4 socket
        IPv4 = AF_INET,
        /// IPv6 socket
        IPv6 = AF_INET6,
        /// Unix socket
        UNIX = AF_UNIX,
    };

    /**
         * @brief 创建TCP Socket(满足地址类型)
         * @param[in] address 地址
         * @return 返回创建的TCP Socket智能指针
         */
    static Socket::ptr CreateTCP(CIM::Address::ptr address);

    /**
         * @brief 创建UDP Socket(满足地址类型)
         * @param[in] address 地址
         * @return 返回创建的UDP Socket智能指针
         */
    static Socket::ptr CreateUDP(CIM::Address::ptr address);

    /**
         * @brief 创建IPv4的TCP Socket
         * @return 返回创建的IPv4 TCP Socket智能指针
         */
    static Socket::ptr CreateTCPSocket();

    /**
         * @brief 创建IPv4的UDP Socket
         * @return 返回创建的IPv4 UDP Socket智能指针
         */
    static Socket::ptr CreateUDPSocket();

    /**
         * @brief 创建IPv6的TCP Socket
         * @return 返回创建的IPv6 TCP Socket智能指针
         */
    static Socket::ptr CreateTCPSocket6();

    /**
         * @brief 创建IPv6的UDP Socket
         * @return 返回创建的IPv6 UDP Socket智能指针
         */
    static Socket::ptr CreateUDPSocket6();

    /**
         * @brief 创建Unix的TCP Socket
         * @return 返回创建的Unix TCP Socket智能指针
         */
    static Socket::ptr CreateUnixTCPSocket();

    /**
         * @brief 创建Unix的UDP Socket
         * @return 返回创建的Unix UDP Socket智能指针
         */
    static Socket::ptr CreateUnixUDPSocket();

    /**
         * @brief Socket构造函数
         * @param[in] family 协议簇
         * @param[in] type 类型
         * @param[in] protocol 协议
         */
    Socket(int family, int type, int protocol = 0);

    /**
         * @brief 析构函数
         */
    virtual ~Socket();

    /**
         * @brief 获取发送超时时间(毫秒)
         * @return 返回发送超时时间(毫秒)，-1表示获取失败
         */
    int64_t getSendTimeout();

    /**
         * @brief 设置发送超时时间(毫秒)
         * @param[in] v 超时时间(毫秒)
         */
    void setSendTimeout(int64_t v);

    /**
         * @brief 获取接受超时时间(毫秒)
         * @return 返回接收超时时间(毫秒)，-1表示获取失败
         */
    int64_t getRecvTimeout();

    /**
         * @brief 设置接受超时时间(毫秒)
         * @param[in] v 超时时间(毫秒)
         */
    void setRecvTimeout(int64_t v);

    /**
         * @brief 获取sockopt @see getsockopt
         * @param[in] level 级别
         * @param[in] option 选项
         * @param[out] result 结果缓冲区
         * @param[in,out] len 缓冲区长度
         * @return 是否获取成功
         */
    bool getOption(int level, int option, void* result, socklen_t* len);

    /**
         * @brief 获取sockopt模板 @see getsockopt
         * @tparam T 结果类型
         * @param[in] level 级别
         * @param[in] option 选项
         * @param[out] result 结果值
         * @return 是否获取成功
         */
    template <class T>
    bool getOption(int level, int option, T& result) {
        socklen_t length = sizeof(T);
        return getOption(level, option, &result, &length);
    }

    /**
         * @brief 设置sockopt @see setsockopt
         * @param[in] level 级别
         * @param[in] option 选项
         * @param[in] result 值
         * @param[in] len 值长度
         * @return 是否设置成功
         */
    bool setOption(int level, int option, const void* result, socklen_t len);

    /**
         * @brief 设置sockopt模板 @see setsockopt
         * @tparam T 值类型
         * @param[in] level 级别
         * @param[in] option 选项
         * @param[in] value 值
         * @return 是否设置成功
         */
    template <class T>
    bool setOption(int level, int option, const T& value) {
        return setOption(level, option, &value, sizeof(T));
    }

    /**
         * @brief 接收connect链接
         * @return 成功返回新连接的socket,失败返回nullptr
         * @pre Socket必须 bind , listen  成功
         */
    virtual Socket::ptr accept();

    /**
         * @brief 绑定地址
         * @param[in] addr 地址
         * @return 是否绑定成功
         */
    virtual bool bind(const Address::ptr addr);

    /**
         * @brief 连接地址
         * @param[in] addr 目标地址
         * @param[in] timeout_ms 超时时间(毫秒)
         * @return 是否连接成功
         */
    virtual bool connect(const Address::ptr addr, uint64_t timeout_ms = -1);

    /**
         * @brief 重新连接
         * @param[in] timeout_ms 超时时间(毫秒)
         * @return 是否连接成功
         */
    virtual bool reconnect(uint64_t timeout_ms = -1);

    /**
         * @brief 监听socket
         * @param[in] backlog 未完成连接队列的最大长度
         * @result 返回监听是否成功
         * @pre 必须先 bind 成功
         */
    virtual bool listen(int backlog = SOMAXCONN);

    /**
         * @brief 关闭socket
         * @return 是否关闭成功
         */
    virtual bool close();

    /**
         * @brief 发送数据
         * @param[in] buffer 待发送数据的内存
         * @param[in] length 待发送数据的长度
         * @param[in] flags 标志字
         * @return
         *      @retval >0 发送成功对应大小的数据
         *      @retval =0 socket被关闭
         *      @retval <0 socket出错
         */
    virtual int send(const void* buffer, size_t length, int flags = 0);

    /**
         * @brief 发送数据
         * @param[in] buffers 待发送数据的内存(iovec数组)
         * @param[in] length 待发送数据的长度(iovec长度)
         * @param[in] flags 标志字
         * @return
         *      @retval >0 发送成功对应大小的数据
         *      @retval =0 socket被关闭
         *      @retval <0 socket出错
         */
    virtual int send(const iovec* buffers, size_t length, int flags = 0);

    /**
         * @brief 发送数据到指定地址
         * @param[in] buffer 待发送数据的内存
         * @param[in] length 待发送数据的长度
         * @param[in] to 发送的目标地址
         * @param[in] flags 标志字
         * @return
         *      @retval >0 发送成功对应大小的数据
         *      @retval =0 socket被关闭
         *      @retval <0 socket出错
         */
    virtual int sendTo(const void* buffer, size_t length, const Address::ptr to, int flags = 0);

    /**
         * @brief 发送数据到指定地址
         * @param[in] buffers 待发送数据的内存(iovec数组)
         * @param[in] length 待发送数据的长度(iovec长度)
         * @param[in] to 发送的目标地址
         * @param[in] flags 标志字
         * @return
         *      @retval >0 发送成功对应大小的数据
         *      @retval =0 socket被关闭
         *      @retval <0 socket出错
         */
    virtual int sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags = 0);

    /**
         * @brief 接受数据
         * @param[out] buffer 接收数据的内存
         * @param[in] length 接收数据的内存大小
         * @param[in] flags 标志字
         * @return
         *      @retval >0 接收到对应大小的数据
         *      @retval =0 socket被关闭
         *      @retval <0 socket出错
         */
    virtual int recv(void* buffer, size_t length, int flags = 0);

    /**
         * @brief 接受数据
         * @param[out] buffers 接收数据的内存(iovec数组)
         * @param[in] length 接收数据的内存大小(iovec数组长度)
         * @param[in] flags 标志字
         * @return
         *      @retval >0 接收到对应大小的数据
         *      @retval =0 socket被关闭
         *      @retval <0 socket出错
         */
    virtual int recv(iovec* buffers, size_t length, int flags = 0);

    /**
         * @brief 接受来自指定地址的数据
         * @param[out] buffer 接收数据的内存
         * @param[in] length 接收数据的内存大小
         * @param[out] from 发送端地址
         * @param[in] flags 标志字
         * @return
         *      @retval >0 接收到对应大小的数据
         *      @retval =0 socket被关闭
         *      @retval <0 socket出错
         */
    virtual int recvFrom(void* buffer, size_t length, Address::ptr from, int flags = 0);

    /**
         * @brief 接受来自指定地址的数据
         * @param[out] buffers 接收数据的内存(iovec数组)
         * @param[in] length 接收数据的内存大小(iovec数组长度)
         * @param[out] from 发送端地址
         * @param[in] flags 标志字
         * @return
         *      @retval >0 接收到对应大小的数据
         *      @retval =0 socket被关闭
         *      @retval <0 socket出错
         */
    virtual int recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags = 0);

    /**
         * @brief 输出信息到流中
         * @param[in,out] os 输出流
         * @return 输出流引用
         */
    virtual std::ostream& dump(std::ostream& os) const;

    /**
         * @brief 转换为字符串
         * @return 字符串表示
         */
    virtual std::string toString() const;

    /**
         * @brief 获取远端地址
         * @return 返回远端地址
         */
    Address::ptr getRemoteAddress();

    /**
         * @brief 获取本地地址
         * @return 返回本地地址
         */
    Address::ptr getLocalAddress();

    /**
         * @brief 获取协议簇
         * @return 返回协议簇
         */
    int getFamily() const;

    /**
         * @brief 获取类型
         * @return 返回类型
         */
    int getType() const;

    /**
         * @brief 获取协议
         * @return 返回协议
         */
    int getProtocol() const;

    /**
         * @brief 返回是否连接
         * @return 是否已连接
         */
    bool isConnected() const;

    /**
         * @brief 是否有效(m_sock != -1)
         * @return socket是否有效
         */
    bool isValid() const;

    /**
         * @brief 返回Socket错误
         * @return 错误码
         */
    int getError();

    /**
         * @brief 返回socket句柄
         * @return socket描述符
         */
    int getSocket() const;

    /**
         * @brief 取消读
         * @return 操作是否成功
         */
    bool cancelRead();

    /**
         * @brief 取消写
         * @return 操作是否成功
         */
    bool cancelWrite();

    /**
         * @brief 取消accept
         * @return 操作是否成功
         */
    bool cancelAccept();

    /**
         * @brief 取消所有事件
         * @return 操作是否成功
         */
    bool cancelAll();

   protected:
    /**
         * @brief 初始化socket
         * @param[in] sock socket描述符
         * @return 是否初始化成功
         */
    virtual bool init(int sock);

    /**
         * @brief 设置socket的默认选项，以优化性能
         */
    void setDefaultOptions();

    /**
         * @brief 创建socket
         */
    void newSock();

   protected:
    int m_sock;                    /// socket句柄
    int m_family;                  /// 协议簇(TCP/UDP)
    int m_type;                    /// 类型(IPv4/IPv6)
    int m_protocol;                /// 协议
    bool m_isConnected;            /// 是否连接
    Address::ptr m_localAddress;   /// 本地地址
    Address::ptr m_remoteAddress;  /// 远端地址
};

/**
     * @brief SSL Socket封装类
     *
     * 继承自Socket类，提供基于SSL/TLS的安全网络通信功能
     */
class SSLSocket : public Socket {
   public:
    typedef std::shared_ptr<SSLSocket> ptr;

    /**
         * @brief 创建TCP SSL Socket(满足地址类型)
         * @param[in] address 地址
         * @return 返回创建的SSL Socket智能指针
         */
    static SSLSocket::ptr CreateTCP(CIM::Address::ptr address);

    /**
         * @brief 创建IPv4的TCP SSL Socket
         * @return 返回创建的IPv4 TCP SSL Socket智能指针
         */
    static SSLSocket::ptr CreateTCPSocket();

    /**
         * @brief 创建IPv6的TCP SSL Socket
         * @return 返回创建的IPv6 TCP SSL Socket智能指针
         */
    static SSLSocket::ptr CreateTCPSocket6();

    /**
         * @brief SSLSocket构造函数
         * @param[in] family 协议簇
         * @param[in] type 类型
         * @param[in] protocol 协议
         */
    SSLSocket(int family, int type, int protocol = 0);

    /**
         * @brief 接收connect链接
         * @return 成功返回新连接的socket,失败返回nullptr
         * @pre Socket必须 bind , listen  成功
         */
    Socket::ptr accept() override;

    /**
         * @brief 绑定地址
         * @param[in] addr 地址
         * @return 是否绑定成功
         */
    bool bind(const Address::ptr addr) override;

    /**
         * @brief 连接地址
         * @param[in] addr 目标地址
         * @param[in] timeout_ms 超时时间(毫秒)
         * @return 是否连接成功
         */
    bool connect(const Address::ptr addr, uint64_t timeout_ms = -1) override;

    /**
         * @brief 监听socket
         * @param[in] backlog 未完成连接队列的最大长度
         * @result 返回监听是否成功
         * @pre 必须先 bind 成功
         */
    bool listen(int backlog = SOMAXCONN) override;

    /**
         * @brief 关闭socket
         * @return 是否关闭成功
         */
    bool close() override;

    /**
         * @brief 发送数据
         * @param[in] buffer 待发送数据的内存
         * @param[in] length 待发送数据的长度
         * @param[in] flags 标志字
         * @return
         *      @retval >0 发送成功对应大小的数据
         *      @retval =0 socket被关闭
         *      @retval <0 socket出错
         */
    int send(const void* buffer, size_t length, int flags = 0) override;

    /**
         * @brief 发送数据
         * @param[in] buffers 待发送数据的内存(iovec数组)
         * @param[in] length 待发送数据的长度(iovec长度)
         * @param[in] flags 标志字
         * @return
         *      @retval >0 发送成功对应大小的数据
         *      @retval =0 socket被关闭
         *      @retval <0 socket出错
         */
    int send(const iovec* buffers, size_t length, int flags = 0) override;

    /**
         * @brief 发送数据到指定地址
         * @param[in] buffer 待发送数据的内存
         * @param[in] length 待发送数据的长度
         * @param[in] to 发送的目标地址
         * @param[in] flags 标志字
         * @return
         *      @retval >0 发送成功对应大小的数据
         *      @retval =0 socket被关闭
         *      @retval <0 socket出错
         */
    int sendTo(const void* buffer, size_t length, const Address::ptr to, int flags = 0) override;

    /**
         * @brief 发送数据到指定地址
         * @param[in] buffers 待发送数据的内存(iovec数组)
         * @param[in] length 待发送数据的长度(iovec长度)
         * @param[in] to 发送的目标地址
         * @param[in] flags 标志字
         * @return
         *      @retval >0 发送成功对应大小的数据
         *      @retval =0 socket被关闭
         *      @retval <0 socket出错
         */
    int sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags = 0) override;

    /**
         * @brief 接受数据
         * @param[out] buffer 接收数据的内存
         * @param[in] length 接收数据的内存大小
         * @param[in] flags 标志字
         * @return
         *      @retval >0 接收到对应大小的数据
         *      @retval =0 socket被关闭
         *      @retval <0 socket出错
         */
    int recv(void* buffer, size_t length, int flags = 0) override;

    /**
         * @brief 接受数据
         * @param[out] buffers 接收数据的内存(iovec数组)
         * @param[in] length 接收数据的内存大小(iovec数组长度)
         * @param[in] flags 标志字
         * @return
         *      @retval >0 接收到对应大小的数据
         *      @retval =0 socket被关闭
         *      @retval <0 socket出错
         */
    int recv(iovec* buffers, size_t length, int flags = 0) override;

    /**
         * @brief 接受来自指定地址的数据
         * @param[out] buffer 接收数据的内存
         * @param[in] length 接收数据的内存大小
         * @param[out] from 发送端地址
         * @param[in] flags 标志字
         * @return
         *      @retval >0 接收到对应大小的数据
         *      @retval =0 socket被关闭
         *      @retval <0 socket出错
         */
    int recvFrom(void* buffer, size_t length, Address::ptr from, int flags = 0) override;

    /**
         * @brief 接受来自指定地址的数据
         * @param[out] buffers 接收数据的内存(iovec数组)
         * @param[in] length 接收数据的内存大小(iovec数组长度)
         * @param[out] from 发送端地址
         * @param[in] flags 标志字
         * @return
         *      @retval >0 接收到对应大小的数据
         *      @retval =0 socket被关闭
         *      @retval <0 socket出错
         */
    int recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags = 0) override;

    /**
         * @brief 加载证书
         * @param[in] cert_file 证书文件路径
         * @param[in] key_file 私钥文件路径
         * @return 是否加载成功
         */
    bool loadCertificates(const std::string& cert_file, const std::string& key_file);

    /**
         * @brief 输出信息到流中
         * @param[in,out] os 输出流
         * @return 输出流引用
         */
    std::ostream& dump(std::ostream& os) const override;

   protected:
    /**
         * @brief 初始化sock
         * @param[in] sock socket描述符
         * @return 是否初始化成功
         */
    bool init(int sock) override;

   private:
    std::shared_ptr<SSL_CTX> m_ctx;  /// SSL上下文
    std::shared_ptr<SSL> m_ssl;      /// SSL对象
};

/**
     * @brief 流式输出socket
     * @param[in, out] os 输出流
     * @param[in] sock Socket类
     * @return 输出流引用
     */
std::ostream& operator<<(std::ostream& os, const Socket& sock);

}  // namespace CIM