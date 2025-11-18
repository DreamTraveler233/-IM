/**
 * @file address.hpp
 * @brief 网络地址封装模块
 *
 * 该文件提供了对各种网络地址类型的封装，包括IPv4、IPv6和Unix域套接字地址。
 * 提供了地址解析、创建、比较等功能，是网络通信的基础模块。
 */

#pragma once

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace CIM {
class IPAddress;

/**
     * @brief 网络地址基类
     *
     * 抽象基类，定义了网络地址的基本接口，所有具体的地址类型都需要继承此类。
     * 提供了地址的创建、比较、转换字符串等基本功能。
     */
class Address {
   public:
    /// 智能指针类型定义
    using ptr = std::shared_ptr<Address>;

    /**
         * @brief 通过sockaddr指针创建Address对象
         * @param[in] addr sockaddr指针
         * @param[in] addrlen sockaddr长度
         * @return 返回创建的Address对象智能指针
         */
    static Address::ptr Create(const sockaddr* addr, socklen_t addrlen);

    /**
         * @brief 通过主机名解析出地址列表
         * @param[out] result 解析结果存储容器
         * @param[in] host 主机名，可以是域名或IP地址
         * @param[in] family 协议族，如AF_INET、AF_INET6等
         * @param[in] type socket类型，如SOCK_STREAM、SOCK_DGRAM等
         * @param[in] protocol 协议类型，如IPPROTO_TCP、IPPROTO_UDP等
         * @return 解析成功返回true，失败返回false
         */
    static bool Lookup(std::vector<Address::ptr>& result, const std::string& host,
                       int family = AF_INET, int type = 0, int protocol = 0);

    /**
         * @brief 解析任意地址
         * @param[in] host 主机名
         * @param[in] family 协议族
         * @param[in] type socket类型
         * @param[in] protocol 协议类型
         * @return 返回解析的第一个地址，解析失败返回nullptr
         */
    static Address::ptr LookupAny(const std::string& host, int family = AF_INET, int type = 0,
                                  int protocol = 0);

    /**
         * @brief 解析任意IP地址
         * @param[in] host 主机名
         * @param[in] family 协议族
         * @param[in] type socket类型
         * @param[in] protocol 协议类型
         * @return 返回解析的第一个IP地址，解析失败返回nullptr
         */
    static std::shared_ptr<IPAddress> LookupAnyIpAddress(const std::string& host,
                                                         int family = AF_INET, int type = 0,
                                                         int protocol = 0);

    /**
         * @brief 获取本地网卡接口地址列表
         * @param[out] result 存储接口地址的multimap容器，key为接口名，value.first为地址，value.second为子网掩码长度
         * @param[in] family 协议族
         * @return 获取成功返回true，失败返回false
         */
    static bool GetInterfaceAddresses(
        std::multimap<std::string, std::pair<Address::ptr, uint32_t>>& result,
        int family = AF_UNSPEC);

    /**
         * @brief 获取指定网卡接口地址列表
         * @param[out] result 存储接口地址的vector容器，pair.first为地址，pair.second为子网掩码长度
         * @param[in] iface 网卡接口名
         * @param[in] family 协议族
         * @return 获取成功返回true，失败返回false
         */
    static bool GetInterfaceAddresses(std::vector<std::pair<Address::ptr, uint32_t>>& result,
                                      const std::string& iface, int family = AF_INET);

    /**
         * @brief 析构函数
         */
    virtual ~Address() {}

    /**
         * @brief 获取地址族
         * @return 地址族，如AF_INET、AF_INET6等
         */
    int getFamily() const;

    /**
         * @brief 获取sockaddr指针（const版本）
         * @return sockaddr指针
         */
    virtual const sockaddr* getAddr() const = 0;

    /**
         * @brief 获取sockaddr指针
         * @return sockaddr指针
         */
    virtual sockaddr* getAddr() = 0;

    /**
         * @brief 获取sockaddr长度
         * @return sockaddr长度
         */
    virtual socklen_t getAddrLen() const = 0;

    /**
         * @brief 插入到输出流
         * @param[inout] os 输出流
         * @return 输出流
         */
    virtual std::ostream& insert(std::ostream& os) const = 0;

    /**
         * @brief 转换为字符串
         * @return 地址的字符串表示
         */
    std::string toString() const;

    /**
         * @brief 小于比较运算符
         * @param[in] rhs 右侧比较对象
         * @return 比较结果
         */
    bool operator<(const Address& rhs) const;

    /**
         * @brief 等于比较运算符
         * @param[in] rhs 右侧比较对象
         * @return 比较结果
         */
    bool operator==(const Address& rhs) const;

    /**
         * @brief 不等于比较运算符
         * @param[in] rhs 右侧比较对象
         * @return 比较结果
         */
    bool operator!=(const Address& rhs) const;
};

/**
     * @brief IP地址类
     *
     * 继承自Address，专门用于表示IP地址（IPv4和IPv6），
     * 提供了广播地址、网络地址、子网掩码等相关操作。
     */
class IPAddress : public Address {
   public:
    /// 智能指针类型定义
    using ptr = std::shared_ptr<IPAddress>;

    /**
         * @brief 创建IP地址对象
         * @param[in] address IP地址字符串
         * @param[in] port 端口号
         * @return 返回创建的IP地址对象智能指针
         */
    static IPAddress::ptr Create(const char* address, uint16_t port = 0);

    /**
         * @brief 获取广播地址
         * @param[in] prefix_len 前缀长度
         * @return 广播地址智能指针
         */
    virtual IPAddress::ptr broadcastAddress(uint32_t prefix_len) = 0;

    /**
         * @brief 获取网络地址
         * @param[in] prefix_len 前缀长度
         * @return 网络地址智能指针
         */
    virtual IPAddress::ptr networkAddress(uint32_t prefix_len) = 0;

    /**
         * @brief 获取子网掩码地址
         * @param[in] prefix_len 前缀长度
         * @return 子网掩码地址智能指针
         */
    virtual IPAddress::ptr subnetMask(uint32_t prefix_len) = 0;

    /**
         * @brief 获取端口号
         * @return 端口号
         */
    virtual uint32_t getPort() const = 0;

    /**
         * @brief 设置端口号
         * @param[in] port 端口号
         */
    virtual void setPort(uint16_t port) = 0;
};

/**
     * @brief IPv4地址类
     *
     * 继承自IPAddress，用于表示IPv4地址。
     */
class IPv4Address : public IPAddress {
   public:
    /// 智能指针类型定义
    using ptr = std::shared_ptr<IPv4Address>;

    /**
         * @brief 创建IPv4地址对象
         * @param[in] address IP地址字符串
         * @param[in] port 端口号
         * @return 返回创建的IPv4地址对象智能指针
         */
    static IPv4Address::ptr Create(const char* address, uint16_t port = 0);

    /**
         * @brief 构造函数
         * @param[in] address sockaddr_in结构体
         */
    IPv4Address(const sockaddr_in& address);

    /**
         * @brief 构造函数
         * @param[in] address IPv4地址，默认为INADDR_ANY
         * @param[in] port 端口号，默认为0
         */
    IPv4Address(uint32_t address = INADDR_ANY, uint16_t port = 0);

    /**
         * @brief 获取sockaddr指针（const版本）
         * @return sockaddr指针
         */
    const sockaddr* getAddr() const override;

    /**
         * @brief 获取sockaddr指针
         * @return sockaddr指针
         */
    sockaddr* getAddr() override;

    /**
         * @brief 获取sockaddr长度
         * @return sockaddr长度
         */
    socklen_t getAddrLen() const override;

    /**
         * @brief 插入到输出流
         * @param[inout] os 输出流
         * @return 输出流
         */
    std::ostream& insert(std::ostream& os) const override;

    /**
         * @brief 获取广播地址
         * @param[in] prefix_len 前缀长度
         * @return 广播地址智能指针
         */
    IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;

    /**
         * @brief 获取网络地址
         * @param[in] prefix_len 前缀长度
         * @return 网络地址智能指针
         */
    IPAddress::ptr networkAddress(uint32_t prefix_len) override;

    /**
         * @brief 获取子网掩码地址
         * @param[in] prefix_len 前缀长度
         * @return 子网掩码地址智能指针
         */
    IPAddress::ptr subnetMask(uint32_t prefix_len) override;

    /**
         * @brief 获取端口号
         * @return 端口号
         */
    uint32_t getPort() const override;

    /**
         * @brief 设置端口号
         * @param[in] port 端口号
         */
    void setPort(uint16_t port) override;

   private:
    /// IPv4地址结构体
    sockaddr_in m_addr;
};

/**
     * @brief IPv6地址类
     *
     * 继承自IPAddress，用于表示IPv6地址。
     */
class IPv6Address : public IPAddress {
   public:
    /// 智能指针类型定义
    using ptr = std::shared_ptr<IPv6Address>;

    /**
         * @brief 创建IPv6地址对象
         * @param[in] address IP地址字符串
         * @param[in] port 端口号
         * @return 返回创建的IPv6地址对象智能指针
         */
    static IPv6Address::ptr Create(const char* address, uint16_t port = 0);

    /**
         * @brief 默认构造函数
         */
    IPv6Address();

    /**
         * @brief 构造函数
         * @param[in] address sockaddr_in6结构体
         */
    IPv6Address(const sockaddr_in6& address);

    /**
         * @brief 构造函数
         * @param[in] address IPv6地址数组
         * @param[in] port 端口号，默认为0
         */
    IPv6Address(const uint8_t address[16], uint16_t port = 0);

    /**
         * @brief 获取sockaddr指针（const版本）
         * @return sockaddr指针
         */
    const sockaddr* getAddr() const override;

    /**
         * @brief 获取sockaddr指针
         * @return sockaddr指针
         */
    sockaddr* getAddr() override;

    /**
         * @brief 获取sockaddr长度
         * @return sockaddr长度
         */
    socklen_t getAddrLen() const override;

    /**
         * @brief 插入到输出流
         * @param[inout] os 输出流
         * @return 输出流
         */
    std::ostream& insert(std::ostream& os) const override;

    /**
         * @brief 获取广播地址
         * @param[in] prefix_len 前缀长度
         * @return 广播地址智能指针
         */
    IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;

    /**
         * @brief 获取网络地址
         * @param[in] prefix_len 前缀长度
         * @return 网络地址智能指针
         */
    IPAddress::ptr networkAddress(uint32_t prefix_len) override;

    /**
         * @brief 获取子网掩码地址
         * @param[in] prefix_len 前缀长度
         * @return 子网掩码地址智能指针
         */
    IPAddress::ptr subnetMask(uint32_t prefix_len) override;

    /**
         * @brief 获取端口号
         * @return 端口号
         */
    uint32_t getPort() const override;

    /**
         * @brief 设置端口号
         * @param[in] port 端口号
         */
    void setPort(uint16_t port) override;

   private:
    /// IPv6地址结构体
    sockaddr_in6 m_addr;
};

/**
     * @brief Unix域套接字地址类
     *
     * 继承自Address，用于表示Unix域套接字地址。
     */
class UnixAddress : public Address {
   public:
    /// 智能指针类型定义
    using ptr = std::shared_ptr<UnixAddress>;

    /**
         * @brief 默认构造函数
         */
    UnixAddress();

    /**
         * @brief 构造函数
         * @param[in] path 路径
         */
    UnixAddress(const std::string& path);

    /**
         * @brief 获取sockaddr指针（const版本）
         * @return sockaddr指针
         */
    const sockaddr* getAddr() const override;

    /**
         * @brief 获取sockaddr指针
         * @return sockaddr指针
         */
    sockaddr* getAddr() override;

    /**
         * @brief 获取sockaddr长度
         * @return sockaddr长度
         */
    socklen_t getAddrLen() const override;

    /**
         * @brief 设置sockaddr长度
         * @param[in] length sockaddr长度
         */
    void setAddrLen(socklen_t length);

    std::string getPath() const;

    /**
         * @brief 插入到输出流
         * @param[inout] os 输出流
         * @return 输出流
         */
    std::ostream& insert(std::ostream& os) const override;

   private:
    /// Unix域套接字地址结构体
    struct sockaddr_un m_addr;
    /// 地址长度
    socklen_t m_length;
};

/**
     * @brief 未知地址类
     *
     * 继承自Address，用于表示未知类型的地址。
     */
class UnknownAddress : public Address {
   public:
    /// 智能指针类型定义
    using ptr = std::shared_ptr<UnknownAddress>;

    /**
         * @brief 构造函数
         * @param[in] family 协议族
         */
    UnknownAddress(int family);

    /**
         * @brief 构造函数
         * @param[in] addr sockaddr结构体
         */
    UnknownAddress(const sockaddr& addr);

    /**
         * @brief 获取sockaddr指针（const版本）
         * @return sockaddr指针
         */
    const sockaddr* getAddr() const override;

    /**
         * @brief 获取sockaddr指针
         * @return sockaddr指针
         */
    sockaddr* getAddr() override;

    /**
         * @brief 获取sockaddr长度
         * @return sockaddr长度
         */
    socklen_t getAddrLen() const override;

    /**
         * @brief 插入到输出流
         * @param[inout] os 输出流
         * @return 输出流
         */
    std::ostream& insert(std::ostream& os) const override;

   private:
    /// 地址结构体
    sockaddr m_addr;
};

/**
     * @brief 流式输出Address
     */
std::ostream& operator<<(std::ostream& os, const Address& addr);
}  // namespace CIM