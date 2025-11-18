#pragma once

#include <memory>

#include "byte_array.hpp"

namespace CIM {
/**
     * @brief 抽象流接口类，定义了流的基本操作
     *
     * Stream类是CIM网络库中所有流式数据传输的基类，
     * 提供了读写数据的统一接口，可以用于文件、网络套接字等各种数据流。
     */
class Stream {
   public:
    typedef std::shared_ptr<Stream> ptr;

    /**
         * @brief 虚析构函数，确保派生类能正确析构
         */
    virtual ~Stream() {}

    /**
         * @brief 从流中读取数据到指定缓冲区
         * @param[out] buffer 接收数据的内存缓冲区
         * @param[in] length 缓冲区大小（期望读取的字节数）
         * @return 实际读取的字节数，0表示流已关闭，负数表示出错
         *
         * @note 这是一个纯虚函数，需要在派生类中实现
         */
    virtual int read(void* buffer, size_t length) = 0;

    /**
         * @brief 从流中读取数据到ByteArray对象
         * @param[out] ba 接收数据的ByteArray智能指针
         * @param[in] length 期望读取的字节数
         * @return 实际读取的字节数，0表示流已关闭，负数表示出错
         *
         * @note 这是一个纯虚函数，需要在派生类中实现
         */
    virtual int read(ByteArray::ptr ba, size_t length) = 0;

    /**
         * @brief 从流中读取固定长度的数据到指定缓冲区
         * @param[out] buffer 接收数据的内存缓冲区
         * @param[in] length 期望读取的字节数
         * @return 实际读取的字节数，0表示流已关闭，负数表示出错
         *
         * @note 与read不同，该函数会尝试读取完整的length字节数据，
         *       如果数据不足会持续等待直到读取完成或出错
         */
    virtual int readFixSize(void* buffer, size_t length);

    /**
         * @brief 从流中读取固定长度的数据到ByteArray对象
         * @param[out] ba 接收数据的ByteArray智能指针
         * @param[in] length 期望读取的字节数
         * @return 实际读取的字节数，0表示流已关闭，负数表示出错
         *
         * @note 与read不同，该函数会尝试读取完整的length字节数据，
         *       如果数据不足会持续等待直到读取完成或出错
         */
    virtual int readFixSize(ByteArray::ptr ba, size_t length);

    /**
         * @brief 向流中写入数据
         * @param[in] buffer 要写入数据的内存缓冲区
         * @param[in] length 要写入的字节数
         * @return 实际写入的字节数，0表示流已关闭，负数表示出错
         *
         * @note 这是一个纯虚函数，需要在派生类中实现
         */
    virtual int write(const void* buffer, size_t length) = 0;

    /**
         * @brief 向流中写入ByteArray对象的数据
         * @param[in] ba 要写入数据的ByteArray智能指针
         * @param[in] length 要写入的字节数
         * @return 实际写入的字节数，0表示流已关闭，负数表示出错
         *
         * @note 这是一个纯虚函数，需要在派生类中实现
         */
    virtual int write(ByteArray::ptr ba, size_t length) = 0;

    /**
         * @brief 向流中写入固定长度的数据
         * @param[in] buffer 要写入数据的内存缓冲区
         * @param[in] length 要写入的字节数
         * @return 实际写入的字节数，0表示流已关闭，负数表示出错
         *
         * @note 与write不同，该函数会尝试写入完整的length字节数据，
         *       如果写入不完整会持续尝试直到完成或出错
         */
    virtual int writeFixSize(const void* buffer, size_t length);

    /**
         * @brief 向流中写入ByteArray对象的固定长度数据
         * @param[in] ba 要写入数据的ByteArray智能指针
         * @param[in] length 要写入的字节数
         * @return 实际写入的字节数，0表示流已关闭，负数表示出错
         *
         * @note 与write不同，该函数会尝试写入完整的length字节数据，
         *       如果写入不完整会持续尝试直到完成或出错
         */
    virtual int writeFixSize(ByteArray::ptr ba, size_t length);

    /**
         * @brief 关闭流
         *
         * @note 这是一个纯虚函数，需要在派生类中实现
         */
    virtual void close() = 0;
};
}  // namespace CIM