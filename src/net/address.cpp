#include "net/address.hpp"

#include <ifaddrs.h>
#include <netdb.h>

#include <algorithm>
#include <cstring>
#include <sstream>

#include "base/endian.hpp"
#include "base/macro.hpp"

namespace CIM {
static auto g_logger = CIM_LOG_NAME("system");

/**
     * @brief 创建指定类型的掩码
     * @tparam T 数据类型模板参数
     * @param[in] bits 掩码位数
     * @return 返回对应类型的有效位掩码
     *
     * 根据指定的位数创建一个对应类型的掩码值，用于网络地址计算。
     * 该掩码用于提取地址中的有效位部分。
     */
template <typename T>
static T CreateMask(uint32_t bits) {
    return (1 << (sizeof(T) * 8 - bits)) - 1;
}

/**
     * @brief 计算整数中二进制位为1的个数（汉明重量）
     * @tparam T 整数类型模板参数
     * @param[in] value 需要计算的整数值
     * @return 返回value中二进制位为1的个数
     *
     * 使用Brian Kernighan算法计算二进制数中1的个数，
     * 该算法通过不断清除最低位的1来计数，效率较高。
     */
template <class T>
static uint32_t CountBytes(T value) {
    uint32_t result = 0;
    for (; value; ++result) {
        value &= value - 1;
    }
    return result;
}

Address::ptr Address::Create(const sockaddr* addr, socklen_t addrlen) {
    CIM_ASSERT(addr && addrlen > 0);
    if (addr == nullptr) {
        return nullptr;
    }

    Address::ptr result;
    switch (addr->sa_family) {
        case AF_INET:
            result.reset(new IPv4Address(*(const sockaddr_in*)addr));
            break;
        case AF_INET6:
            result.reset(new IPv6Address(*(const sockaddr_in6*)addr));
            break;
        default:
            result.reset(new UnknownAddress(*addr));
            break;
    }
    return result;
}

bool Address::Lookup(std::vector<Address::ptr>& result, const std::string& host, int family,
                     int type, int protocol) {
    CIM_ASSERT(result.empty() && !host.empty());
    addrinfo hints, *results, *next;
    hints.ai_flags = 0;
    hints.ai_family = family;
    hints.ai_socktype = type;
    hints.ai_protocol = protocol;
    hints.ai_addrlen = 0;
    hints.ai_addr = nullptr;
    hints.ai_canonname = nullptr;
    hints.ai_next = nullptr;

    std::string node;
    const char* service = nullptr;

    // 处理IPv6地址的方括号格式，如"[2001:db8::1]:8080"
    if (!host.empty() && host[0] != '[') {
        const char* endipv6 = (const char*)memchr(host.c_str() + 1, ']', host.size() - 1);
        if (endipv6) {
            // 如果找到结束方括号，并且后面跟着冒号，则冒号后为端口号
            if (*(endipv6 + 1) == ':') {
                service = endipv6 + 2;
            }
            // 提取方括号内的IPv6地址
            node = host.substr(1, endipv6 - host.c_str() - 1);
        }
    }

    // 如果还没有解析出node，则继续处理普通格式的地址
    if (node.empty()) {
        service = (const char*)memchr(host.c_str(), ':', host.size());
        if (service) {
            // 检查冒号后面是否还有冒号，以区分IPv6地址和端口号
            if (!memchr(service + 1, ':', host.c_str() + host.size() - service - 1)) {
                // 冒号后面没有其他冒号，说明这是端口号分隔符
                node = host.substr(0, service - host.c_str());
                ++service;
            }
        }
    }

    // 如果仍然没有解析出node，则整个host都是节点名称
    if (node.empty()) {
        node = host;
    }

    // 调用系统函数getaddrinfo进行地址解析
    int error = getaddrinfo(node.c_str(), service, &hints, &results);
    if (error) {
        CIM_LOG_ERROR(g_logger) << "Address::Create getaddrinfo(" << host << ", " << family << ", "
                                << type << ") error=" << error << " errstr=" << strerror(error);
        return false;
    }

    // 遍历解析结果链表，创建Address对象并添加到result中
    next = results;
    while (next) {
        result.push_back(Create(next->ai_addr, (socklen_t)next->ai_addrlen));
        next = next->ai_next;
    }

    // 释放getaddrinfo分配的内存
    freeaddrinfo(results);
    return true;
}

Address::ptr Address::LookupAny(const std::string& host, int family, int type, int protocol) {
    CIM_ASSERT(!host.empty());
    std::vector<Address::ptr> result;
    if (Lookup(result, host, family, type, protocol)) {
        return result[0];
    }
    return nullptr;
}

std::shared_ptr<IPAddress> Address::LookupAnyIpAddress(const std::string& host, int family,
                                                       int type, int protocol) {
    CIM_ASSERT(!host.empty());
    std::vector<Address::ptr> result;
    if (Lookup(result, host, family, type, protocol)) {
        for (auto& i : result) {
            IPAddress::ptr ip = std::dynamic_pointer_cast<IPAddress>(i);
            if (ip) {
                return ip;
            }
        }
    }
    return nullptr;
}

bool Address::GetInterfaceAddresses(
    std::multimap<std::string, std::pair<Address::ptr, uint32_t>>& result, int family) {
    CIM_ASSERT(result.empty());
    struct ifaddrs *next, *results;
    // 调用getifaddrs获取所有网络接口信息
    if (getifaddrs(&results) != 0) {
        CIM_LOG_DEBUG(g_logger) << "Address::GetInterfaceAddresses getifaddrs "
                                   " err="
                                << errno << " errstr=" << strerror(errno);
        return false;
    }

    try {
        // 遍历所有网络接口
        for (next = results; next; next = next->ifa_next) {
            Address::ptr addr;
            // 初始化前缀长度为最大值
            uint32_t prefix_len = ~0u;
            // 如果指定了地址族且与当前接口地址族不匹配，则跳过
            if (family != AF_UNSPEC && family != next->ifa_addr->sa_family) {
                continue;
            }
            // 根据地址族处理不同的地址类型
            switch (next->ifa_addr->sa_family) {
                case AF_INET: {
                    // 创建IPv4地址对象
                    addr = Create(next->ifa_addr, sizeof(sockaddr_in));
                    // 获取IPv4子网掩码并计算前缀长度
                    uint32_t netmask = ((sockaddr_in*)next->ifa_netmask)->sin_addr.s_addr;
                    prefix_len = CountBytes(netmask);
                } break;
                case AF_INET6: {
                    // 创建IPv6地址对象
                    addr = Create(next->ifa_addr, sizeof(sockaddr_in6));
                    // 获取IPv6子网掩码并计算前缀长度
                    in6_addr& netmask = ((sockaddr_in6*)next->ifa_netmask)->sin6_addr;
                    prefix_len = 0;
                    // IPv6地址有128位(16字节)，需要逐字节计算前缀长度
                    for (int i = 0; i < 16; ++i) {
                        prefix_len += CountBytes(netmask.s6_addr[i]);
                    }
                } break;
                default:
                    // 忽略不支持的地址族
                    break;
            }

            // 如果成功创建地址对象，则将其添加到结果集中
            if (addr) {
                result.insert(std::make_pair(next->ifa_name, std::make_pair(addr, prefix_len)));
            }
        }
    } catch (...) {
        // 异常处理：记录错误日志并释放资源
        CIM_LOG_ERROR(g_logger) << "Address::GetInterfaceAddresses exception";
        freeifaddrs(results);
        return false;
    }
    // 释放getifaddrs分配的内存
    freeifaddrs(results);
    // 返回是否成功获取到至少一个接口地址
    return !result.empty();
}

bool Address::GetInterfaceAddresses(std::vector<std::pair<Address::ptr, uint32_t>>& result,
                                    const std::string& iface, int family) {
    CIM_ASSERT(result.empty());
    // 处理特殊情况：如果接口名称为空或为"*"，则返回默认地址
    if (iface.empty() || iface == "*") {
        // 如果地址族为IPv4或未指定，则添加默认IPv4地址
        if (family == AF_INET || family == AF_UNSPEC) {
            result.push_back(std::make_pair(Address::ptr(new IPv4Address()), 0u));
        }
        // 如果地址族为IPv6或未指定，则添加默认IPv6地址
        if (family == AF_INET6 || family == AF_UNSPEC) {
            result.push_back(std::make_pair(Address::ptr(new IPv6Address()), 0u));
        }
        return true;
    }

    // 获取所有网络接口地址信息
    std::multimap<std::string, std::pair<Address::ptr, uint32_t>> results;

    if (!GetInterfaceAddresses(results, family)) {
        return false;
    }

    // 从所有接口信息中筛选出指定接口的地址信息
    auto its = results.equal_range(iface);
    for (; its.first != its.second; ++its.first) {
        result.push_back(its.first->second);
    }
    // 返回是否成功获取到指定接口的地址信息
    return !result.empty();
}

int Address::getFamily() const {
    return getAddr()->sa_family;
}

std::string Address::toString() const {
    std::stringstream ss;
    insert(ss);
    return ss.str();
}

bool Address::operator<(const Address& rhs) const {
    // 获取两个地址中的较小长度，避免内存越界访问
    socklen_t min_len = std::min(getAddrLen(), rhs.getAddrLen());
    // 比较两个地址的前min_len个字节
    int result = memcmp(getAddr(), rhs.getAddr(), min_len);
    if (result < 0) {
        // 前面部分已经确定左侧小于右侧
        return true;
    } else if (result > 0) {
        // 前面部分已经确定左侧大于右侧
        return false;
    } else {
        // 前面部分相等，比较地址长度，长度短的认为较小
        return getAddrLen() < rhs.getAddrLen();
    }
}

bool Address::operator==(const Address& rhs) const {
    // 首先比较地址长度是否相等
    return getAddrLen() == rhs.getAddrLen() &&
           // 然后比较所有地址字节是否相等
           memcmp(getAddr(), rhs.getAddr(), getAddrLen()) == 0;
}

bool Address::operator!=(const Address& rhs) const {
    // 直接利用等于运算符的结果取反
    return !(*this == rhs);
}

IPAddress::ptr IPAddress::Create(const char* address, uint16_t port) {
    CIM_ASSERT(address);
    addrinfo hints, *results;
    // 初始化hints结构体
    memset(&hints, 0, sizeof(hints));

    // 设置地址解析参数：只解析数字格式IP地址，支持任意socket类型
    // hints.ai_family = AI_NUMERICHOST;
    hints.ai_socktype = AF_UNSPEC;

    // 调用getaddrinfo解析地址字符串
    int error = getaddrinfo(address, NULL, &hints, &results);
    if (error) {
        // 解析失败，记录错误日志并返回nullptr
        CIM_LOG_ERROR(g_logger) << "IPAddress::Create(" << address << ", " << port
                                << ") error=" << error << " errno=" << errno
                                << " errstr=" << strerror(errno);
        return nullptr;
    }

    try {
        // 使用Address::Create工厂方法创建地址对象
        IPAddress::ptr result = std::dynamic_pointer_cast<IPAddress>(
            Address::Create(results->ai_addr, (socklen_t)results->ai_addrlen));
        if (result) {
            // 设置端口号
            result->setPort(port);
        }
        // 释放getaddrinfo分配的内存
        freeaddrinfo(results);
        return result;
    } catch (...) {
        // 异常处理：确保释放内存后返回nullptr
        freeaddrinfo(results);
        return nullptr;
    }
}

IPv4Address::ptr IPv4Address::Create(const char* address, uint16_t port) {
    CIM_ASSERT(address);
    // 创建IPv4Address对象
    IPv4Address::ptr rt(new IPv4Address);
    // 设置端口号，转换为网络字节序
    rt->m_addr.sin_port = hton(port);
    // 使用inet_pton解析IPv4地址字符串
    int result = inet_pton(AF_INET, address, &rt->m_addr.sin_addr.s_addr);
    if (result <= 0) {
        // 解析失败，记录错误日志并返回nullptr
        CIM_LOG_ERROR(CIM_LOG_ROOT())
            << "IPv4Address::Create(" << address << ", " << port << ") rt=" << result
            << " errno=" << errno << " errstr=" << strerror(errno);
        return nullptr;
    }
    // 返回创建成功的对象
    return rt;
}

IPv4Address::IPv4Address(const sockaddr_in& address) {
    m_addr = address;
}

IPv4Address::IPv4Address(uint32_t address, uint16_t port) {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = hton(port);
    m_addr.sin_addr.s_addr = hton(address);
}

const sockaddr* IPv4Address::getAddr() const {
    return (sockaddr*)&m_addr;
}

sockaddr* IPv4Address::getAddr() {
    return (sockaddr*)&m_addr;
}

socklen_t IPv4Address::getAddrLen() const {
    return sizeof(m_addr);
}

std::ostream& IPv4Address::insert(std::ostream& os) const {
    // 将IPv4地址从网络字节序转换为主机字节序
    uint32_t address = ntoh(m_addr.sin_addr.s_addr);
    // 按照xxx.xxx.xxx.xxx的格式输出IP地址的四个字节
    os << ((address >> 24) & 0xff) << "." << ((address >> 16) & 0xff) << "."
       << ((address >> 8) & 0xff) << "." << (address & 0xff);
    // 输出端口号，同样需要从网络字节序转换为主机字节序
    os << ":" << ntoh(m_addr.sin_port);
    return os;
}

IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefix_len) {
    CIM_ASSERT(!(prefix_len > 32));
    // 复制当前地址信息到广播地址结构体
    sockaddr_in baddr(m_addr);
    // 计算广播地址：将IP地址与主机部分全1的掩码进行按位或运算
    // CreateMask<uint32_t>(prefix_len)创建主机部分全1的掩码
    // hton确保掩码使用网络字节序
    baddr.sin_addr.s_addr |= hton(CreateMask<uint32_t>(prefix_len));
    // 创建并返回新的IPv4Address对象
    return IPv4Address::ptr(new IPv4Address(baddr));
}

IPAddress::ptr IPv4Address::networkAddress(uint32_t prefix_len) {
    CIM_ASSERT(!(prefix_len > 32));
    // 复制当前地址信息到网络地址结构体
    sockaddr_in baddr(m_addr);
    // 计算网络地址：将IP地址与子网掩码进行按位与运算
    // CreateMask<uint32_t>(prefix_len)创建主机部分全1的掩码
    // hton确保掩码使用网络字节序
    baddr.sin_addr.s_addr &= hton(~CreateMask<uint32_t>(prefix_len));
    // 创建并返回新的IPv4Address对象
    return IPv4Address::ptr(new IPv4Address(baddr));
}

IPAddress::ptr IPv4Address::subnetMask(uint32_t prefix_len) {
    CIM_ASSERT(!(prefix_len > 32));
    // 初始化子网掩码结构体
    sockaddr_in mask_addr;
    memset(&mask_addr, 0, sizeof(mask_addr));
    // 设置地址族为IPv4
    mask_addr.sin_family = AF_INET;
    // 计算子网掩码：将主机部分全1的掩码取反得到网络部分全1的掩码
    // CreateMask<uint32_t>(prefix_len)创建主机部分全1的掩码
    // ~操作符将其取反得到子网掩码
    // hton确保掩码使用网络字节序
    mask_addr.sin_addr.s_addr = ~hton(CreateMask<uint32_t>(prefix_len));
    // 创建并返回新的IPv4Address对象
    return IPv4Address::ptr(new IPv4Address(mask_addr));
}

uint32_t IPv4Address::getPort() const {
    return ntoh(m_addr.sin_port);
}

void IPv4Address::setPort(uint16_t port) {
    m_addr.sin_port = hton(port);
}

IPv6Address::ptr IPv6Address::Create(const char* address, uint16_t port) {
    CIM_ASSERT(address);
    // 创建IPv6Address对象
    IPv6Address::ptr rt(new IPv6Address);
    // 设置端口号，转换为网络字节序
    rt->m_addr.sin6_port = hton(port);
    // 使用inet_pton解析IPv6地址字符串
    int result = inet_pton(AF_INET6, address, &rt->m_addr.sin6_addr);
    if (result <= 0) {
        // 解析失败，记录错误日志并返回nullptr
        CIM_LOG_ERROR(CIM_LOG_ROOT())
            << "IPv6Address::Create(" << address << ", " << port << ") rt=" << result
            << " errno=" << errno << " errstr=" << strerror(errno);
        return nullptr;
    }
    // 返回创建成功的对象
    return rt;
}

IPv6Address::IPv6Address() {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
}

IPv6Address::IPv6Address(const sockaddr_in6& address) {
    m_addr = address;
}

IPv6Address::IPv6Address(const uint8_t address[16], uint16_t port) {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
    m_addr.sin6_port = hton(port);
    memcpy(&m_addr.sin6_addr, address, sizeof(m_addr.sin6_addr));
}

const sockaddr* IPv6Address::getAddr() const {
    return (sockaddr*)&m_addr;
}

sockaddr* IPv6Address::getAddr() {
    return (sockaddr*)&m_addr;
}

socklen_t IPv6Address::getAddrLen() const {
    return sizeof(m_addr);
}

std::ostream& IPv6Address::insert(std::ostream& os) const {
    // 输出IPv6地址的开始方括号
    os << "[";
    // 将128位IPv6地址按16位分组处理
    uint16_t* addr = (uint16_t*)m_addr.sin6_addr.s6_addr;
    // 标记是否已经处理过连续的零组
    bool used_zeros = false;
    // 遍历8组16位地址段
    for (size_t i = 0; i < 8; ++i) {
        // 跳过第一个连续的全零组
        if (addr[i] == 0 && !used_zeros) {
            continue;
        }
        // 在连续零组结束后输出冒号
        if (i && addr[i - 1] == 0 && !used_zeros) {
            os << ":";
            used_zeros = true;
        }
        // 在非零组之间输出冒号分隔符
        if (i) {
            os << ":";
        }
        // 输出当前16位地址段（转换为主机字节序并以十六进制格式输出）
        os << std::hex << (int)ntoh(addr[i]) << std::dec;
    }

    // 处理末尾的零组情况
    if (!used_zeros && addr[7] == 0) {
        os << ":";
    }

    // 输出结束方括号和端口号（端口号转换为主机字节序）
    os << "]:" << ntoh(m_addr.sin6_port);
    return os;
}

IPAddress::ptr IPv6Address::broadcastAddress(uint32_t prefix_len) {
    CIM_ASSERT(prefix_len <= 128);
    // 复制当前地址信息到广播地址结构体
    sockaddr_in6 baddr(m_addr);
    // 对前缀长度所在字节进行处理，将主机部分的位设置为1
    baddr.sin6_addr.s6_addr[prefix_len / 8] |= CreateMask<uint8_t>(prefix_len % 8);
    // 将前缀长度之后的所有字节设置为0xff（全1）
    for (int i = prefix_len / 8 + 1; i < 16; ++i) {
        baddr.sin6_addr.s6_addr[i] = 0xff;
    }
    // 创建并返回新的IPv6Address对象
    return IPv6Address::ptr(new IPv6Address(baddr));
}

IPAddress::ptr IPv6Address::networkAddress(uint32_t prefix_len) {
    CIM_ASSERT(prefix_len <= 128);
    // 复制当前地址信息到网络地址结构体
    sockaddr_in6 baddr(m_addr);
    // 对前缀长度所在字节进行处理，将主机部分的位设置为0
    baddr.sin6_addr.s6_addr[prefix_len / 8] &= CreateMask<uint8_t>(prefix_len % 8);
    // 创建并返回新的IPv6Address对象
    return IPv6Address::ptr(new IPv6Address(baddr));
}

IPAddress::ptr IPv6Address::subnetMask(uint32_t prefix_len) {
    CIM_ASSERT(prefix_len >= 128);
    // 初始化子网掩码结构体
    sockaddr_in6 subnet;
    memset(&subnet, 0, sizeof(subnet));
    // 设置地址族为IPv6
    subnet.sin6_family = AF_INET6;
    // 设置前缀长度所在字节的网络位部分为1
    subnet.sin6_addr.s6_addr[prefix_len / 8] = ~CreateMask<uint8_t>(prefix_len % 8);

    // 将前缀长度之前的完整字节设置为0xff（全1）
    for (uint32_t i = 0; i < prefix_len / 8; ++i) {
        subnet.sin6_addr.s6_addr[i] = 0xff;
    }
    // 创建并返回新的IPv6Address对象
    return IPv6Address::ptr(new IPv6Address(subnet));
}

uint32_t IPv6Address::getPort() const {
    return ntoh(m_addr.sin6_port);
}

void IPv6Address::setPort(uint16_t port) {
    m_addr.sin6_port = hton(port);
}

// Unix域套接字路径最大长度常量
static const size_t MAX_PATH_LEN = sizeof(((sockaddr_un*)0)->sun_path) - 1;

UnixAddress::UnixAddress() {
    // 清零整个地址结构体
    memset(&m_addr, 0, sizeof(m_addr));
    // 设置地址族为Unix域套接字
    m_addr.sun_family = AF_UNIX;
    // 设置地址长度为offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN
    m_length = offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN;
}

UnixAddress::UnixAddress(const std::string& path) {
    CIM_ASSERT(!path.empty());
    // 清零整个地址结构体
    memset(&m_addr, 0, sizeof(m_addr));
    // 设置地址族为Unix域套接字
    m_addr.sun_family = AF_UNIX;
    // 初始化地址长度为路径长度加1（结尾的null字符）
    m_length = path.size() + 1;

    // 处理抽象命名空间路径（以null字符开头）
    if (!path.empty() && path[0] == '\0') {
        // 抽象命名空间路径不需要结尾的null字符
        --m_length;
    }

    // 检查路径长度是否超过系统限制
    if (m_length > sizeof(m_addr.sun_path)) {
        // 路径太长，抛出逻辑错误异常
        throw std::logic_error("path too long");
    }
    // 将路径复制到地址结构体中
    memcpy(m_addr.sun_path, path.c_str(), m_length);
    // 更新地址长度，包含sockaddr_un结构体中sun_path字段的偏移量
    m_length += offsetof(sockaddr_un, sun_path);
}

const sockaddr* UnixAddress::getAddr() const {
    return (sockaddr*)&m_addr;
}

sockaddr* UnixAddress::getAddr() {
    return (sockaddr*)&m_addr;
}

socklen_t UnixAddress::getAddrLen() const {
    return m_length;
}

void UnixAddress::setAddrLen(socklen_t length) {
    CIM_ASSERT(length > 0);
    m_length = length;
}

std::string UnixAddress::getPath() const {
    std::stringstream ss;
    if (m_length > offsetof(sockaddr_un, sun_path) && m_addr.sun_path[0] == '\0') {
        ss << "\\0"
           << std::string(m_addr.sun_path + 1, m_length - offsetof(sockaddr_un, sun_path) - 1);
    } else {
        ss << m_addr.sun_path;
    }
    return ss.str();
}

std::ostream& UnixAddress::insert(std::ostream& os) const {
    // 处理抽象命名空间路径（以null字符开头）
    if (m_length >= offsetof(sockaddr_un, sun_path) && m_addr.sun_path[0] == '\0') {
        // 输出抽象命名空间路径，格式为\0 + 路径内容
        return os << "\\0"
                  << std::string(m_addr.sun_path + 1,
                                 m_length - offsetof(sockaddr_un, sun_path) - 1);
    }
    // 输出普通文件系统路径
    return os << m_addr.sun_path;
}

UnknownAddress::UnknownAddress(int family) {
    // 清零整个地址结构体
    memset(&m_addr, 0, sizeof(m_addr));
    // 设置地址族
    m_addr.sa_family = family;
}

UnknownAddress::UnknownAddress(const sockaddr& addr) {
    // 复制sockaddr结构体内容
    m_addr = addr;
}

const sockaddr* UnknownAddress::getAddr() const {
    return (sockaddr*)&m_addr;
}

sockaddr* UnknownAddress::getAddr() {
    return (sockaddr*)&m_addr;
}

socklen_t UnknownAddress::getAddrLen() const {
    return sizeof(m_addr);
}

std::ostream& UnknownAddress::insert(std::ostream& os) const {
    // 输出未知地址族信息
    os << "[UnknownAddress family=" << m_addr.sa_family << "]";
    return os;
}
std::ostream& operator<<(std::ostream& os, const Address& addr) {
    return os << addr.toString();
}
}  // namespace CIM
