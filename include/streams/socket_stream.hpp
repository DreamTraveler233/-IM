#pragma once

#include "io/iomanager.hpp"
#include "net/socket.hpp"
#include "net/stream.hpp"

namespace CIM {
/**
     * @brief Socket流
     */
class SocketStream : public Stream {
   public:
    typedef std::shared_ptr<SocketStream> ptr;

    /**
         * @brief 构造函数
         * @param[in] sock Socket类
         * @param[in] owner 是否完全控制
         */
    SocketStream(Socket::ptr sock, bool owner = true);

    /**
         * @brief 析构函数，负责关闭套接字
         * @details 如果m_owner=true,则close
         */
    ~SocketStream();

    /**
         * @brief 从 Socket 读取数据到指定内存缓冲区
         * @param[out] buffer 待接收数据的内存
         * @param[in] length 待接收数据的内存长度
         * @return
         *      @retval >0 返回实际接收到的数据长度
         *      @retval =0 socket被远端关闭
         *      @retval <0 socket错误
         */
    virtual int read(void* buffer, size_t length) override;

    /**
         * @brief 从 Socket 读取数据到 ByteArray
         * @param[out] ba 接收数据的ByteArray
         * @param[in] length 待接收数据的内存长度
         * @return
         *      @retval >0 返回实际接收到的数据长度
         *      @retval =0 socket被远端关闭
         *      @retval <0 socket错误
         */
    virtual int read(ByteArray::ptr ba, size_t length) override;

    /**
         * @brief 将内存缓冲区数据写入 Socket
         * @param[in] buffer 待发送数据的内存
         * @param[in] length 待发送数据的内存长度
         * @return
         *      @retval >0 返回实际接收到的数据长度
         *      @retval =0 socket被远端关闭
         *      @retval <0 socket错误
         */
    virtual int write(const void* buffer, size_t length) override;

    /**
         * @brief 将 ByteArray 中的数据写入 Socket
         * @param[in] ba 待发送数据的ByteArray
         * @param[in] length 待发送数据的内存长度
         * @return
         *      @retval >0 返回实际接收到的数据长度
         *      @retval =0 socket被远端关闭
         *      @retval <0 socket错误
         */
    virtual int write(ByteArray::ptr ba, size_t length) override;

    /**
         * @brief 关闭socket
         */
    virtual void close() override;

    /**
         * @brief 返回Socket类
         */
    Socket::ptr getSocket() const;

    /**
         * @brief 返回是否连接
         */
    bool isConnected() const;

    /**
         * @brief 获取远端地址
         */
    Address::ptr getRemoteAddress();

    /**
         * @brief 获取本地地址
         */
    Address::ptr getLocalAddress();

    /**
         * @brief 获取远端地址字符串
         */
    std::string getRemoteAddressString();

    /**
         * @brief 返回本地地址字符串
         */
    std::string getLocalAddressString();

   protected:
    Socket::ptr m_socket;  /// Socket类
    bool m_owner;          /// 是否主控
};
}  // namespace CIM