/**
 * @file fd_manager.hpp
 * @brief 文件描述符管理模块
 * @author your_name
 * @date 2025-10-24
 *
 * 该文件实现了文件描述符的统一管理机制，包括：
 * 1. FdCtx: 文件描述符上下文信息管理
 * 2. FdManager: 全局文件描述符管理器
 * 3. FileDescriptor: RAII风格的文件描述符封装
 *
 * 主要功能：
 * - 管理文件描述符的初始化状态、类型(socket或普通文件)、阻塞模式等属性
 * - 提供文件描述符超时时间的设置和获取
 * - 实现线程安全的文件描述符获取和删除操作
 * - 提供RAII机制自动管理文件描述符生命周期
 */

#ifndef __CIM_FD_MANAGER_H__
#define __CIM_FD_MANAGER_H__

#include "io/iomanager.hpp"
#include "io/lock.hpp"
#include "memory"
#include "base/noncopyable.hpp"
#include "base/singleton.hpp"

namespace CIM {
/**
     * @brief 文件描述符上下文类
     * @details 用于存储和管理单个文件描述符的相关信息，
     *          包括初始化状态、是否为socket、阻塞模式、超时时间等
     */
class FdCtx : public std::enable_shared_from_this<FdCtx> {
   public:
    using ptr = std::shared_ptr<FdCtx>;  ///< 智能指针类型定义

    /**
         * @brief 构造函数
         * @param[in] fd 文件描述符
         */
    FdCtx(int fd);

    /**
         * @brief 析构函数
         */
    ~FdCtx() = default;

    /**
         * @brief 初始化文件描述符上下文
         * @return 初始化成功返回true，失败返回false
         */
    bool init();

    /**
         * @brief 检查文件描述符是否已初始化
         * @return 已初始化返回true，否则返回false
         */
    bool isInit() const;

    /**
         * @brief 检查文件描述符是否为socket类型
         * @return 是socket返回true，否则返回false
         */
    bool isSocket() const;

    /**
         * @brief 检查文件描述符是否已关闭
         * @return 已关闭返回true，否则返回false
         */
    bool isClose() const;

    /**
         * @brief 关闭文件描述符
         * @return 操作成功返回true，失败返回false
         */
    bool close();

    /**
         * @brief 设置用户非阻塞模式
         * @param[in] v true表示设置为非阻塞，false表示阻塞模式
         */
    void setUserNonBlock(bool v);

    /**
         * @brief 获取用户非阻塞模式设置
         * @return true表示设置为非阻塞，false表示阻塞模式
         */
    bool getUserNonBlock() const;

    /**
         * @brief 设置系统非阻塞模式
         * @param[in] v true表示设置为非阻塞，false表示阻塞模式
         */
    void setSysNonBlock(bool v);

    /**
         * @brief 获取系统非阻塞模式设置
         * @return true表示设置为非阻塞，false表示阻塞模式
         */
    bool getSysNonBlock() const;

    /**
         * @brief 设置超时时间
         * @param[in] type 超时类型，SO_RCVTIMEO表示接收超时，SO_SNDTIMEO表示发送超时
         * @param[in] v 超时时间(毫秒)
         */
    void setTimeout(int type, uint64_t v);

    /**
         * @brief 获取超时时间
         * @param[in] type 超时类型，SO_RCVTIMEO表示接收超时，SO_SNDTIMEO表示发送超时
         * @return 超时时间(毫秒)
         */
    uint64_t getTimeout(int type) const;

   private:
    bool m_isInit : 1;        // 占用1个bit，表示对象是否已初始化
    bool m_isSocket : 1;      // 占用1个bit，表示是否为socket文件描述符
    bool m_sysNonBlock : 1;   // 占用1个bit，表示系统层面是否设置了非阻塞模式
    bool m_userNonBlock : 1;  // 占用1个bit，表示用户是否设置了非阻塞模式
    bool m_isClosed : 1;      // 占用1个bit，表示文件描述符是否已关闭
    int m_fd;                 // 文件描述符
    uint64_t m_recvTimeout;   // 接收超时时间
    uint64_t m_sendTimeout;   // 发送超时时间
};

/**
     * @brief 文件描述符管理器类
     * @details 全局管理所有文件描述符的上下文信息，提供线程安全的访问接口
     */
class FdManager {
   public:
    using ptr = std::shared_ptr<FdManager>;  ///< 智能指针类型定义
    using RWMutexType = RWMutex;             ///< 读写锁类型定义

    /**
         * @brief 构造函数
         */
    FdManager();

    /**
         * @brief 获取文件描述符上下文
         * @param[in] fd 文件描述符
         * @param[in] auto_create 是否自动创建，默认为false
         * @return 文件描述符上下文智能指针
         */
    FdCtx::ptr get(int fd, bool auto_create = false);

    /**
         * @brief 删除文件描述符上下文
         * @param[in] fd 文件描述符
         */
    void del(int fd);

   private:
    std::vector<FdCtx::ptr> m_fdCtxs;  ///< 文件描述符上下文容器
    RWMutexType m_mutex;               ///< 读写锁，保护容器访问安全
};

using FdMgr = Singleton<FdManager>;  ///< 全局单例模式的文件描述符管理器

/**
     * @brief RAII文件描述符管理类
     * @details 自动管理文件描述符的生命周期，在对象析构时自动关闭文件描述符
     */
class FileDescriptor : public Noncopyable {
   public:
    /**
         * @brief 构造函数
         * @param fd 文件描述符
         */
    explicit FileDescriptor(int fd = -1);

    /**
         * @brief 析构函数，自动关闭文件描述符
         */
    ~FileDescriptor();

    /**
         * @brief 获取文件描述符
         * @return int 文件描述符
         */
    int get() const;

    /**
         * @brief 重置文件描述符
         * @param fd 新的文件描述符
         */
    void reset(int fd = -1);

    /**
         * @brief 释放文件描述符所有权
         * @return int 文件描述符
         */
    int release();

    /**
         * @brief 检查文件描述符是否有效
         * @return bool 文件描述符是否有效
         */
    bool isValid() const;

    // 支持移动
    FileDescriptor(FileDescriptor&& other) noexcept;
    FileDescriptor& operator=(FileDescriptor&& other) noexcept;

   private:
    int m_fd;  ///< 文件描述符
};
}  // namespace CIM

#endif  // __CIM_FD_MANAGER_H__