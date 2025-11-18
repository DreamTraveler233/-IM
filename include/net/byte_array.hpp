/**
 * @file byte_array.hpp
 * @brief 字节数组类定义文件
 * @details 该文件定义了ByteArray类，用于处理可变长度的字节数据流，支持多种数据类型的读写操作。
 * ByteArray使用链表结构管理内存块，能够动态扩展容量，并支持高效的序列化和反序列化操作。
 * 提供了固定长度和变长编码两种整数写入方式，以及对大小端字节序的支持。
 * 
 * 主要功能包括：
 * 1. 支持基本数据类型（整数、浮点数、字符串）的读写
 * 2. 支持固定长度和变长编码的整数读写
 * 3. 支持大小端字节序转换
 * 4. 支持从文件读取和写入文件
 * 5. 支持以十六进制格式显示数据
 * 6. 提供iovec缓冲区列表用于网络传输
 * 
 * 设计特点：
 * - 使用链表管理内存块，避免频繁的内存重新分配
 * - 支持随机访问和顺序访问
 * - 提供读写位置控制
 * - 线程不安全，需要外部同步机制
 */

#pragma once

#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <memory>
#include <vector>

namespace CIM {
/**
     * @brief 字节数组类
     * @details ByteArray是一个用于处理字节流数据的类，内部使用链表结构管理多个固定大小的内存块。
     * 支持多种数据类型的序列化和反序列化操作，包括整数、浮点数和字符串等。
     */
class ByteArray {
   public:
    using ptr = std::shared_ptr<ByteArray>;

    /**
         * @brief 内存块节点结构
         * @details 每个节点代表一个固定大小的内存块，用于存储实际数据
         */
    struct Node {
        /**
             * @brief 构造函数，创建指定大小的节点
             * @param[in] size 节点大小
             */
        Node(size_t size);

        /**
             * @brief 默认构造函数
             */
        Node();

        /**
             * @brief 析构函数，释放节点内存
             */
        ~Node();

        char* ptr;    //!< 节点数据指针
        Node* next;   //!< 下一个节点指针
        size_t size;  //!< 节点大小
    };

    /**
         * @brief 构造函数
         * @param[in] base_size 基础块大小，默认为4096字节
         */
    ByteArray(size_t base_size = 4096);

    /**
         * @brief 析构函数，释放所有节点内存
         */
    ~ByteArray();

    /**
         * @brief 写入8位有符号整数（固定长度）
         * @param[in] value 要写入的值
         * @details 直接将8位有符号整数写入字节数组
         */
    void writeFint8(int8_t value);

    /**
         * @brief 写入8位无符号整数（固定长度）
         * @param[in] value 要写入的值
         * @details 直接将8位无符号整数写入字节数组
         */
    void writeFuint8(uint8_t value);

    /**
         * @brief 写入16位有符号整数（固定长度）
         * @param[in] value 要写入的值
         * @details 根据字节序设置进行字节序转换后写入字节数组
         */
    void writeFint16(int16_t value);

    /**
         * @brief 写入16位无符号整数（固定长度）
         * @param[in] value 要写入的值
         * @details 根据字节序设置进行字节序转换后写入字节数组
         */
    void writeFuint16(uint16_t value);

    /**
         * @brief 写入32位有符号整数（固定长度）
         * @param[in] value 要写入的值
         * @details 根据字节序设置进行字节序转换后写入字节数组
         */
    void writeFint32(int32_t value);

    /**
         * @brief 写入32位无符号整数（固定长度）
         * @param[in] value 要写入的值
         * @details 根据字节序设置进行字节序转换后写入字节数组
         */
    void writeFuint32(uint32_t value);

    /**
         * @brief 写入64位有符号整数（固定长度）
         * @param[in] value 要写入的值
         * @details 根据字节序设置进行字节序转换后写入字节数组
         */
    void writeFint64(int64_t value);

    /**
         * @brief 写入64位无符号整数（固定长度）
         * @param[in] value 要写入的值
         * @details 根据字节序设置进行字节序转换后写入字节数组
         */
    void writeFuint64(uint64_t value);

    /**
         * @brief 写入32位有符号整数（Zigzag编码+变长编码）
         * @param[in] value 要写入的值
         * @details 先使用Zigzag编码处理有符号整数，再使用变长编码方式写入
         */
    void writeInt32(int32_t value);

    /**
         * @brief 写入32位无符号整数（变长编码）
         * @param[in] value 要写入的值
         * @details 使用变长编码(Varint)方式存储32位无符号整数
         */
    void writeUint32(uint32_t value);

    /**
         * @brief 写入64位有符号整数（Zigzag编码+变长编码）
         * @param[in] value 要写入的值
         * @details 先使用Zigzag编码处理有符号整数，再使用变长编码方式写入
         */
    void writeInt64(int64_t value);

    /**
         * @brief 写入64位无符号整数（变长编码）
         * @param[in] value 要写入的值
         * @details 使用变长编码(Varint)方式存储64位无符号整数
         */
    void writeUint64(uint64_t value);

    /**
         * @brief 写入单精度浮点数
         * @param[in] value 要写入的值
         * @details 将float类型的值转换为uint32_t类型后写入字节数组
         */
    void writeFloat(float value);

    /**
         * @brief 写入双精度浮点数
         * @param[in] value 要写入的值
         * @details 将double类型的值转换为uint64_t类型后写入字节数组
         */
    void writeDouble(double value);

    /**
         * @brief 写入字符串，使用16位整数表示长度
         * @param[in] value 要写入的字符串
         * @details 先写入字符串长度(16位无符号整数)，然后写入字符串内容
         */
    void writeStringF16(const std::string& value);

    /**
         * @brief 写入字符串，使用32位整数表示长度
         * @param[in] value 要写入的字符串
         * @details 先写入字符串长度(32位无符号整数)，然后写入字符串内容
         */
    void writeStringF32(const std::string& value);

    /**
         * @brief 写入字符串，使用64位整数表示长度
         * @param[in] value 要写入的字符串
         * @details 先写入字符串长度(64位无符号整数)，然后写入字符串内容
         */
    void writeStringF64(const std::string& value);

    /**
         * @brief 写入字符串，使用64位变长编码整数表示长度
         * @param[in] value 要写入的字符串
         * @details 先写入字符串长度(64位无符号整数，变长编码)，然后写入字符串内容
         */
    void writeStringVint(const std::string& value);

    /**
         * @brief 写入字符串，不写入长度字段
         * @param[in] value 要写入的字符串
         * @details 直接将字符串内容写入字节数组，不包含长度信息
         */
    void writeStringWithoutLength(const std::string& value);

    /**
         * @brief 读取8位有符号整数
         * @return 读取到的8位有符号整数
         * @details 直接从字节数组中读取8位有符号整数
         */
    int8_t readFint8();

    /**
         * @brief 读取8位无符号整数
         * @return 读取到的8位无符号整数
         * @details 直接从字节数组中读取8位无符号整数
         */
    uint8_t readFuint8();

    /**
         * @brief 读取16位有符号整数
         * @return 读取到的16位有符号整数
         * @details 从字节数组中读取16位有符号整数，并根据字节序设置进行字节序转换
         */
    int16_t readFint16();

    /**
         * @brief 读取16位无符号整数
         * @return 读取到的16位无符号整数
         * @details 从字节数组中读取16位无符号整数，并根据字节序设置进行字节序转换
         */
    uint16_t readFuint16();

    /**
         * @brief 读取32位有符号整数
         * @return 读取到的32位有符号整数
         * @details 从字节数组中读取32位有符号整数，并根据字节序设置进行字节序转换
         */
    int32_t readFint32();

    /**
         * @brief 读取32位无符号整数
         * @return 读取到的32位无符号整数
         * @details 从字节数组中读取32位无符号整数，并根据字节序设置进行字节序转换
         */
    uint32_t readFuint32();

    /**
         * @brief 读取64位有符号整数
         * @return 读取到的64位有符号整数
         * @details 从字节数组中读取64位有符号整数，并根据字节序设置进行字节序转换
         */
    int64_t readFint64();

    /**
         * @brief 读取64位无符号整数
         * @return 读取到的64位无符号整数
         * @details 从字节数组中读取64位无符号整数，并根据字节序设置进行字节序转换
         */
    uint64_t readFuint64();

    /**
         * @brief 读取32位有符号整数（变长编码）
         * @return 解码后的32位有符号整数
         * @details 先从字节数组中读取变长编码的无符号整数，再进行Zigzag解码得到有符号整数
         */
    int32_t readInt32();

    /**
         * @brief 读取32位无符号整数（变长编码）
         * @return 解码后的32位无符号整数
         * @details 从字节数组中读取变长编码的32位无符号整数
         */
    uint32_t readUint32();

    /**
         * @brief 读取64位有符号整数（变长编码）
         * @return 解码后的64位有符号整数
         * @details 先从字节数组中读取变长编码的无符号整数，再进行Zigzag解码得到有符号整数
         */
    int64_t readInt64();

    /**
         * @brief 读取64位无符号整数（变长编码）
         * @return 解码后的64位无符号整数
         * @details 从字节数组中读取变长编码的64位无符号整数
         */
    uint64_t readUint64();

    /**
         * @brief 读取单精度浮点数
         * @return 读取到的单精度浮点数
         * @details 先以整数形式读取4字节数据，再通过内存拷贝转换为float类型
         */
    float readFloat();

    /**
         * @brief 读取双精度浮点数
         * @return 读取到的双精度浮点数
         * @details 先以整数形式读取8字节数据，再通过内存拷贝转换为double类型
         */
    double readDouble();

    /**
         * @brief 读取字符串，使用16位整数表示长度
         * @return 读取到的字符串
         * @details 先读取16位无符号整数作为字符串长度，然后根据该长度读取对应数量的字节构成字符串
         */
    std::string readString16();

    /**
         * @brief 读取字符串，使用32位整数表示长度
         * @return 读取到的字符串
         * @details 先读取32位无符号整数作为字符串长度，然后根据该长度读取对应数量的字节构成字符串
         */
    std::string readString32();

    /**
         * @brief 读取字符串，使用64位整数表示长度
         * @return 读取到的字符串
         * @details 先读取64位无符号整数作为字符串长度，然后根据该长度读取对应数量的字节构成字符串
         */
    std::string readString64();

    /**
         * @brief 读取字符串，使用变长编码整数表示长度
         * @return 读取到的字符串
         * @details 先读取变长编码的无符号整数作为字符串长度，然后根据该长度读取对应数量的字节构成字符串
         */
    std::string readStringVint();

    /**
         * @brief 清空字节数组
         * @details 重置读写位置和数据大小，保留根节点，删除其他所有节点
         */
    void clear();

    /**
         * @brief 写入数据到字节数组
         * @param[in] buf 数据缓冲区指针
         * @param[in] size 数据大小（字节数）
         */
    void write(const void* buf, size_t size);

    /**
         * @brief 从字节数组读取数据
         * @param[out] buf 数据缓冲区指针
         * @param[in] size 要读取的数据大小（字节数）
         */
    void read(void* buf, size_t size);

    /**
         * @brief 从指定位置读取数据（不改变当前读写位置）
         * @param[out] buf 数据缓冲区指针
         * @param[in] size 要读取的数据大小（字节数）
         * @param[in] position 起始读取位置
         */
    void read(void* buf, size_t size, size_t position) const;

    /**
         * @brief 获取当前读写位置
         * @return 当前读写位置
         */
    size_t getPosition() const;

    /**
         * @brief 设置读写位置
         * @param[in] v 新的读写位置
         */
    void setPosition(size_t v);

    /**
         * @brief 将字节数组内容写入文件
         * @param[in] path 文件路径
         * @return 写入成功返回true，失败返回false
         */
    bool writeToFile(const std::string& path) const;

    /**
         * @brief 从文件读取内容到字节数组
         * @param[in] path 文件路径
         * @return 读取成功返回true，失败返回false
         */
    bool readFromFile(const std::string& path);

    /**
         * @brief 获取基础块大小
         * @return 基础块大小
         */
    size_t getBaseSize() const;

    /**
         * @brief 获取可读数据大小
         * @return 可读数据大小（字节数）
         */
    size_t getReadSize() const;

    /**
         * @brief 判断是否为小端字节序
         * @return 小端字节序返回true，大端字节序返回false
         */
    bool isLittleEndian() const;

    /**
         * @brief 设置字节序
         * @param[in] val true表示小端字节序，false表示大端字节序
         */
    void setIsLittleEndian(bool val);

    /**
         * @brief 将字节数组内容转换为字符串
         * @return 字节数组内容对应的字符串
         */
    std::string toString() const;

    /**
         * @brief 将字节数组内容转换为十六进制字符串
         * @return 字节数组内容的十六进制字符串表示
         */
    std::string toHexString() const;

    /**
         * @brief 获取当前可读数据的缓冲区列表
         * @param[out] buffer 存储缓冲区的iovec结构列表
         * @param[in] len 请求读取的最大长度，默认为最大值
         * @return 实际可读的字节数
         */
    uint64_t getReadBuffers(std::vector<iovec>& buffer, uint64_t len = ~0ull);

    /**
         * @brief 获取指定位置的可读数据缓冲区列表
         * @param[out] buffer 存储缓冲区的iovec结构列表
         * @param[in] len 请求读取的最大长度
         * @param[in] position 起始读取位置
         * @return 实际可读的字节数
         */
    uint64_t getReadBuffers(std::vector<iovec>& buffer, uint64_t len, uint64_t position) const;

    /**
         * @brief 获取当前可写数据的缓冲区列表
         * @param[out] buffer 存储缓冲区的iovec结构列表
         * @param[in] len 请求写入的最大长度
         * @return 实际可写的字节数
         */
    uint64_t getWriteBuffers(std::vector<iovec>& buffer, uint64_t len);

    /**
         * @brief 获取数据总大小
         * @return 数据总大小（字节数）
         */
    size_t getDataSize() const;

   private:
    /**
         * @brief 动态增加字节数组的容量
         * @param[in] size 请求增加的容量大小
         */
    void addCapacity(size_t size);

    /**
         * @brief 获取剩余容量
         * @return 剩余容量（字节数）
         */
    size_t getRemainingCapacity() const;

   private:
    size_t m_base_size;  //!< 基础块大小
    size_t m_position;   //!< 当前读写位置
    size_t m_capacity;   //!< 总容量
    size_t m_data_size;  //!< 数据总大小
    int8_t m_endian;     //!< 字节序（CIM_LITTLE_ENDIAN 或 CIM_BIG_ENDIAN）
    Node* m_root_node;   //!< 根节点指针
    Node* m_cur_node;    //!< 当前节点指针
};
}  // namespace CIM