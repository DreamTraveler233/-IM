#include "net/byte_array.hpp"

#include <iomanip>

#include "base/endian.hpp"
#include "base/macro.hpp"

namespace CIM {
static auto g_logger = CIM_LOG_NAME("system");

ByteArray::Node::Node(size_t s) : ptr(new char[s]), next(nullptr), size(s) {}

ByteArray::Node::Node() : ptr(nullptr), next(nullptr), size(0) {}

ByteArray::Node::~Node() {
    if (ptr) {
        delete[] ptr;
    }
}

ByteArray::ByteArray(size_t base_size)
    : m_base_size(base_size),
      m_position(0),
      m_capacity(base_size),
      m_data_size(0),
      m_endian(CIM_BIG_ENDIAN),
      m_root_node(new Node(base_size)),
      m_cur_node(m_root_node) {}

ByteArray::~ByteArray() {
    Node* tmp = m_root_node;
    while (tmp) {
        m_cur_node = tmp;
        tmp = tmp->next;
        delete m_cur_node;
    }
}

void ByteArray::writeFint8(int8_t value) {
    write(&value, sizeof(value));
}

void ByteArray::writeFuint8(uint8_t value) {
    write(&value, sizeof(value));
}

#define XX(value)                     \
    if (CIM_BYTE_ORDER != m_endian) { \
        value = byteswap(value);      \
    }                                 \
    write(&value, sizeof(value));
void ByteArray::writeFint16(int16_t value) {
    XX(value);
}
void ByteArray::writeFuint16(uint16_t value) {
    XX(value);
}
void ByteArray::writeFint32(int32_t value) {
    XX(value);
}
void ByteArray::writeFuint32(uint32_t value) {
    XX(value);
}
void ByteArray::writeFint64(int64_t value) {
    XX(value);
}
void ByteArray::writeFuint64(uint64_t value) {
    XX(value);
}
#undef XX

/**
     * @brief 对32位有符号整数进行Zigzag编码
     * @details Zigzag编码是一种将有符号整数映射为无符号整数的编码方式，
     * 可以使负数的编码变得更紧凑。其映射规则为：
     * 0 -> 0, -1 -> 1, 1 -> 2, -2 -> 3, 2 -> 4...
     * 这样可以使得绝对值较小的负数也能用较少的字节表示
     * @param v 需要编码的32位有符号整数
     * @return 编码后的32位无符号整数
     */
static uint32_t EncodeZigzag32(int32_t v) {
    if (v < 0) {
        return ((uint32_t)(-v)) * 2 - 1;
    } else {
        return (uint32_t)(v) * 2;
    }
}

/**
     * @brief 对64位有符号整数进行Zigzag编码
     * @details Zigzag编码是一种将有符号整数映射为无符号整数的编码方式，
     * 可以使负数的编码变得更紧凑。其映射规则为：
     * 0 -> 0, -1 -> 1, 1 -> 2, -2 -> 3, 2 -> 4...
     * 这样可以使得绝对值较小的负数也能用较少的字节表示
     * @param v 需要编码的64位有符号整数
     * @return 编码后的64位无符号整数
     */
static uint64_t EncodeZigzag64(int64_t v) {
    if (v < 0) {
        return ((uint64_t)(-v)) * 2 - 1;
    } else {
        return (uint64_t)(v) * 2;
    }
}

/**
     * @brief 对32位Zigzag编码的整数进行解码
     * @details 将Zigzag编码后的32位无符号整数还原为原始的有符号整数
     * @param v 需要解码的32位无符号整数
     * @return 解码后的32位有符号整数
     */
static int32_t DecodeZigzag32(uint32_t v) {
    return (v >> 1) ^ -(v & 1);
}

/**
     * @brief 对64位Zigzag编码的整数进行解码
     * @details 将Zigzag编码后的64位无符号整数还原为原始的有符号整数
     * @param v 需要解码的64位无符号整数
     * @return 解码后的64位有符号整数
     */
static int64_t DecodeZigzag64(uint64_t v) {
    return (v >> 1) ^ -(v & 1);
}

/**
     * @brief 写入32位有符号整数(Zigzag编码+变长编码)
     * @details 使用Zigzag编码处理有符号整数，使得小负数也能用较少的字节表示，
     *          然后通过变长编码方式写入
     * @param[in] value 需要写入的32位有符号整数
     */
void ByteArray::writeInt32(int32_t value) {
    // 使用Zigzag编码处理有符号整数，使得小负数也能用较少的字节表示
    // 然后调用writeUint32将编码后的值以变长编码方式写入
    writeUint32(EncodeZigzag32(value));
}

/**
     * @brief 写入32位无符号整数(变长编码)
     * @details 使用变长编码(Varint)方式高效地存储32位无符号整数。
     *          Varint是一种使用一个或多个字节表示整数的方法：
     *          - 每个字节的最高位（msb）是一个标志位，表示是否还有后续字节。
     *            如果该位为1，则表示下一个字节仍然是这个数字的一部分；
     *            如果为0，则表示这是最后一个字节。
     *          - 剩余的7位用于存储实际的数据。
     *          这种编码方式对较小的数值非常高效，例如：
     *          - 数值1会被编码为单个字节: 0000 0001
     *          - 数值300会被编码为两个字节: 1010 1100 0000 0010
     *          因此，小的整数只占用1个字节，而大的整数最多占用5个字节。
     * @param[in] value 需要写入的32位无符号整数
     */
void ByteArray::writeUint32(uint32_t value) {
    // 使用变长编码方式存储32位无符号整数，最多需要5个字节
    // 每个字节使用低7位存储数据，最高位表示是否还有后续字节(1表示有，0表示没有)
    uint8_t tmp[5];
    uint8_t i = 0;

    // 当value大于等于0x80(128)时，需要多个字节存储
    // 每次取value的低7位，并将最高位设置为1(表示还有后续字节)，然后右移7位处理高位
    while (value >= 0x80) {
        // 取value的低7位，设置最高位为1表示还有后续字节
        tmp[i++] = (value & 0x7F) | 0x80;
        // 右移7位处理下一个7位
        value >>= 7;
    }

    // 最后一个字节，最高位为0表示结束
    tmp[i++] = value;

    // 将编码后的字节序列写入ByteArray
    write(tmp, i);
}

void ByteArray::writeInt64(int64_t value) {
    writeUint64(EncodeZigzag64(value));
}

void ByteArray::writeUint64(uint64_t value) {
    uint8_t tmp[10];
    uint8_t i = 0;
    while (value >= 0x80) {
        tmp[i++] = (value & 0x7F) | 0x80;
        value >>= 7;
    }
    tmp[i++] = value;
    write(tmp, i);
}

/**
     * @brief 将float类型的值写入字节数组
     * @param[in] value 需要写入的float值
     * @details 函数首先将float值转换为uint32_t类型，然后调用writeFuint32将其写入字节数组
     */
void ByteArray::writeFloat(float value) {
    uint32_t v;
    memcpy(&v, &value, sizeof(value));
    writeFuint32(v);
}

/**
     * @brief 将double类型的值写入字节数组
     * @param[in] value 需要写入的double值
     * @details 函数首先将double值转换为uint64_t类型，然后调用writeFuint64将其写入字节数组
     */
void ByteArray::writeDouble(double value) {
    uint64_t v;
    memcpy(&v, &value, sizeof(value));
    writeFuint64(v);
}

/**
     * @brief 写入字符串，使用16位整数表示长度，格式为 “ 长度 + 数据 ”
     * @param[in] value 需要写入的字符串
     * @details 首先写入字符串长度(16位无符号整数)，然后写入字符串内容
     */
void ByteArray::writeStringF16(const std::string& value) {
    writeFuint16(value.size());          // 写入长度
    write(value.c_str(), value.size());  // 写入数据
}

void ByteArray::writeStringF32(const std::string& value) {
    writeFuint32(value.size());
    write(value.c_str(), value.size());
}

void ByteArray::writeStringF64(const std::string& value) {
    writeFuint64(value.size());
    write(value.c_str(), value.size());
}

/**
     * @brief 写入字符串，使用64位变长编码整数表示长度，格式为"长度+数据"
     * @param[in] value 需要写入的字符串
     * @details 首先写入字符串长度(64位无符号整数)，然后写入字符串内容
     */
void ByteArray::writeStringVint(const std::string& value) {
    writeUint64(value.size());  // 使用变长编码存储长度
    write(value.c_str(), value.size());
}

/**
     * @brief 写入字符串，不写入长度字段
     * @param[in] value 需要写入的字符串
     * @details 直接将字符串内容写入字节数组，不包含长度信息
     */
void ByteArray::writeStringWithoutLength(const std::string& value) {
    write(value.c_str(), value.size());
}

/**
     * @brief 从字节数组中读取8位有符号整数
     * @return 读取到的8位有符号整数
     */
int8_t ByteArray::readFint8() {
    int8_t v;
    read(&v, sizeof(v));
    return v;
}

uint8_t ByteArray::readFuint8() {
    uint8_t v;
    read(&v, sizeof(v));
    return v;
}

#define XX(type)                      \
    type v;                           \
    read(&v, sizeof(v));              \
    if (CIM_BYTE_ORDER != m_endian) { \
        return byteswap(v);           \
    }                                 \
    return v;
int16_t ByteArray::readFint16() {
    XX(int16_t);
}
uint16_t ByteArray::readFuint16() {
    XX(uint16_t);
}
int32_t ByteArray::readFint32() {
    XX(int32_t);
}
uint32_t ByteArray::readFuint32() {
    XX(uint32_t);
}
int64_t ByteArray::readFint64() {
    XX(int64_t);
}
uint64_t ByteArray::readFuint64() {
    XX(uint64_t);
}
#undef XX

int32_t ByteArray::readInt32() {
    return DecodeZigzag32(readUint32());
}

/**
     * @brief 从字节数组中读取32位无符号变长整数
     * @details 使用变长编码方式读取uint32_t类型数据，每个字节的最高位作为延续位，
     *          当该位为0时表示数据结束，为1时表示还有后续字节
     * @return 解码后的32位无符号整数
     */
uint32_t ByteArray::readUint32() {
    uint32_t result = 0;
    // 按7位组进行解码，最多处理32位数据
    for (int i = 0; i < 32; i += 7) {
        uint8_t b = readFuint8();
        // 如果最高位为0，表示这是最后一个字节
        if (b < 0x80) {
            result |= ((uint32_t)b) << i;
            break;
        } else {
            // 否则取出低7位数据并添加到结果中
            result |= ((uint32_t)(b & 0x7f)) << i;
        }
    }
    return result;
}

int64_t ByteArray::readInt64() {
    return DecodeZigzag64(readUint64());
}

uint64_t ByteArray::readUint64() {
    uint64_t result = 0;
    for (int i = 0; i < 64; i += 7) {
        uint8_t b = readFuint8();
        if (b < 0x80) {
            result |= ((uint64_t)b) << i;
            break;
        } else {
            result |= ((uint64_t)(b & 0x7f)) << i;
        }
    }
    return result;
}

float ByteArray::readFloat() {
    // 先以整数形式读取4字节数据，再通过内存拷贝转换为float类型
    uint32_t v = readFuint32();
    float value;
    memcpy(&value, &v, sizeof(v));
    return value;
}

double ByteArray::readDouble() {
    uint64_t v = readFuint64();
    double value;
    memcpy(&value, &v, sizeof(v));
    return value;
}

/**
     * @brief 读取字符串，使用16位整数表示长度
     * @details 首先读取16位无符号整数作为字符串长度，然后根据该长度读取对应数量的字节构成字符串
     * @return 读取到的字符串
     */
std::string ByteArray::readString16() {
    uint16_t len = readFuint16();
    std::string buffer;
    buffer.resize(len);
    read(&buffer[0], len);
    return buffer;
}

std::string ByteArray::readString32() {
    uint32_t len = readFuint32();
    std::string buffer;
    buffer.resize(len);
    read(&buffer[0], len);
    return buffer;
}

std::string ByteArray::readString64() {
    uint64_t len = readFuint64();
    std::string buffer;
    buffer.resize(len);
    read(&buffer[0], len);
    return buffer;
}

std::string ByteArray::readStringVint() {
    uint64_t len = readUint64();
    std::string buffer;
    buffer.resize(len);
    read(&buffer[0], len);
    return buffer;
}

void ByteArray::clear() {
    m_position = m_data_size = 0;
    m_capacity = m_base_size;
    Node* tmp = m_root_node->next;
    while (tmp) {
        m_cur_node = tmp;
        tmp = tmp->next;
        delete m_cur_node;
    }
    m_cur_node = m_root_node;
    m_root_node->next = nullptr;
}

/**
     * @brief 将数据写入字节数组
     * @param[in] buf 数据缓冲区指针
     * @param[in] size 数据大小（字节数）
     * @details
     * - 确保字节数组有足够的容量存储数据，如果不足则动态扩容。
     * - 数据按块写入链表节点，支持跨节点写入。
     * - 更新写入位置和数据大小。
     */
void ByteArray::write(const void* buf, size_t size) {
    if (!buf || size == 0) {
        return;
    }
    // 确保有足够的容量来写入数据
    addCapacity(size);

    size_t npos = m_position % m_base_size;  // 内存块中的位置偏移
    size_t ncap = m_cur_node->size - npos;   // 当前节点的剩余容量
    size_t bpos = 0;                         // buf的读取位置

    // 循环写入数据直到全部写入完成
    while (size > 0) {
        if (ncap >= size)  // 当前节点可以容纳所有数据
        {
            // 直接将剩余的所有数据拷贝到当前节点
            memcpy(m_cur_node->ptr + npos, (const char*)buf + bpos, size);
            // 如果当前节点已完全写满，则将当前节点指向下一个节点
            if (m_cur_node->size == (npos + size)) {
                m_cur_node = m_cur_node->next;
            }
            // 更新位置信息
            m_position += size;
            bpos += size;
            size = 0;
        } else {
            // 如果当前节点无法完全容纳剩余数据，先填满当前节点
            memcpy(m_cur_node->ptr + npos, (const char*)buf + bpos, ncap);
            // 更新位置信息
            m_position += ncap;
            bpos += ncap;
            size -= ncap;
            // 移动到下一个节点继续写入
            m_cur_node = m_cur_node->next;
            // 重置下一个节点的可用容量和位置偏移
            ncap = m_cur_node->size;
            npos = 0;
        }
    }

    // 如果当前位置超过了已记录的数据大小，则更新数据大小
    if (m_position > m_data_size) {
        m_data_size = m_position;
    }
}

/**
     * @brief 从字节数组中读取数据
     * @param[out] buf 数据缓冲区指针
     * @param[in] size 要读取的数据大小（字节数）
     * @details
     * - 检查是否有足够的数据可供读取。
     * - 数据按块从链表节点中读取，支持跨节点读取。
     * - 更新读取位置。
     * @exception std::out_of_range 如果读取大小超出可用数据范围，抛出异常。
     */
void ByteArray::read(void* buf, size_t size) {
    if (!buf || size == 0) {
        return;
    }

    if (size > getReadSize()) {
        throw std::out_of_range("not enough len");
    }

    size_t npos = m_position % m_base_size;
    size_t ncap = m_cur_node->size - npos;
    size_t bpos = 0;
    while (size > 0) {
        if (ncap >= size)  // 可一次性读完
        {
            memcpy((char*)buf + bpos, m_cur_node->ptr + npos, size);
            // 如果当前节点数据已完全读完，则将当前节点指向下一个节点
            if (m_cur_node->size == (npos + size)) {
                m_cur_node = m_cur_node->next;
            }
            m_position += size;
            bpos += size;
            size = 0;
        } else {
            memcpy((char*)buf + bpos, m_cur_node->ptr + npos, ncap);
            m_position += ncap;
            bpos += ncap;
            size -= ncap;
            m_cur_node = m_cur_node->next;
            ncap = m_cur_node->size;
            npos = 0;
        }
    }
}

/**
     * @brief 从指定位置读取数据
     * @param[out] buf 数据缓冲区指针
     * @param[in] size 要读取的数据大小（字节数）
     * @param[in] position 起始读取位置
     * @details
     * - 从指定位置开始读取数据，不影响当前读写位置。
     * - 数据按块从链表节点中读取，支持跨节点读取。
     * @exception std::out_of_range 如果读取大小超出可用数据范围，抛出异常。
     */
void ByteArray::read(void* buf, size_t size, size_t position) const {
    // 参数检查：如果缓冲区为空或要读取的大小为0，则直接返回
    if (!buf || size == 0) {
        return;
    }

    // 检查是否有足够的数据可读
    if (size > (m_data_size - position)) {
        throw std::out_of_range("not enough len");
    }

    // 从根节点开始查找position所在的节点
    Node* cur = m_root_node;
    size_t count = position / m_base_size;
    while (count > 0) {
        cur = cur->next;
        --count;
    }

    // 计算在当前节点中的位置和剩余容量
    size_t npos = position % m_base_size;
    size_t ncap = cur->size - npos;
    size_t bpos = 0;

    // 循环读取数据直到完成
    while (size > 0) {
        // 如果当前节点有足够的数据满足剩余读取需求
        if (ncap >= size) {
            memcpy((char*)buf + bpos, cur->ptr + npos, size);
            if (cur->size == (npos + size)) {
                cur = cur->next;
            }
            position += size;
            bpos += size;
            size = 0;
        } else {
            // 当前节点数据不足，先读取当前节点所有可用数据
            memcpy((char*)buf + bpos, cur->ptr + npos, ncap);
            position += ncap;
            bpos += ncap;
            size -= ncap;
            cur = cur->next;
            ncap = cur->size;
            npos = 0;
        }
    }
}

size_t ByteArray::getPosition() const {
    return m_position;
}

/**
     * @brief 设置读写位置
     * @param[in] v 新的读写位置
     * @pre v不能超过数据总大小
     * @exception std::out_of_range 当v超出有效范围时抛出
     */
void ByteArray::setPosition(size_t v) {
    if (v > m_capacity) {
        throw std::out_of_range("ByteArray::setPosition out of range");
    }

    m_position = v;

    // 更新实际数据大小
    if (m_position > m_data_size) {
        m_data_size = m_position;
    }

    m_cur_node = m_root_node;
    // 查找position所在节点
    while (v >= m_cur_node->size) {
        v -= m_cur_node->size;
        m_cur_node = m_cur_node->next;
    }
    // 处理边界情况：当position刚好在节点边界时，将当前节点指向下一个节点
    if (v == m_cur_node->size && m_cur_node->next) {
        m_cur_node = m_cur_node->next;
    }
}

/**
     * @brief 将字节数组内容写入文件
     * @param[in] path 文件路径
     * @details
     * - 按当前读写位置开始，将字节数组中的数据写入指定文件。
     * - 数据以二进制模式写入，覆盖文件原有内容。
     * - 如果写入失败，记录错误日志。
     * @return 写入成功返回true，失败返回false
     */
bool ByteArray::writeToFile(const std::string& path) const {
    std::ofstream ofs;
    // 以二进制模式和截断模式（如果文件已存在，则清空文件内容）打开文件
    ofs.open(path, std::ios::trunc | std::ios::binary);
    if (!ofs) {
        CIM_LOG_ERROR(g_logger) << "ByteArray::writeToFile path=" << path << " error=" << errno
                                << " errstr=" << strerror(errno);
        return false;
    }

    int64_t read_size = getReadSize();  // 当前可读数据大小
    int64_t pos = m_position;           // 当前读取位置
    Node* cur = m_cur_node;             // 当前节点

    while (read_size > 0) {
        // 计算从当前节点什么位置开始读取数据
        int diff = pos % m_base_size;
        // 计算本次需要写入的数据长度
        int64_t len = std::min((int64_t)m_base_size - diff, read_size);
        // 写入数据
        ofs.write(cur->ptr + diff, len);
        // 更新索引
        cur = cur->next;   // 读取下一节点
        pos += len;        // 当前位置增加len
        read_size -= len;  // 可读数据减少len
    }

    return true;
}

/**
     * @brief 从文件读取内容到字节数组
     * @param[in] path 文件路径
     * @details
     * - 以二进制模式打开文件，从文件中读取数据并写入字节数组。
     * - 如果读取失败，记录错误日志。
     * - 文件内容会追加到字节数组的当前写入位置。
     * @return 读取成功返回true，失败返回false
     */
bool ByteArray::readFromFile(const std::string& path) {
    std::ifstream ifs;
    // 以二进制模式和读取模式打开文件
    ifs.open(path, std::ios::in | std::ios::binary);
    if (!ifs) {
        CIM_LOG_ERROR(g_logger) << "ByteArray::readFromFile path=" << path << " error=" << errno
                                << " errstr=" << strerror(errno);
        return false;
    }

    std::vector<char> buff(m_base_size);

    // 循环读取文件内容直到文件末尾
    while (!ifs.eof()) {
        // 从文件中读取数据到缓冲区，最多读取m_base_size个字节
        ifs.read(buff.data(), m_base_size);
        // 将缓冲区中的数据写入ByteArray，写入长度为实际读取到的字节数
        write(buff.data(), ifs.gcount());
    }
    return true;
}

void ByteArray::setIsLittleEndian(bool val) {
    if (val) {
        m_endian = CIM_LITTLE_ENDIAN;
    } else {
        m_endian = CIM_BIG_ENDIAN;
    }
}

/**
     * @brief 将字节数组内容转换为字符串
     * @details
     * - 读取当前可读范围内的字节数据并转换为字符串。
     * - 不改变当前读写位置。
     * @return 字节数组内容对应的字符串
     */
std::string ByteArray::toString() const {
    std::string str;
    str.resize(getReadSize());

    // 没有可读数据
    if (str.empty()) {
        return str;
    }

    read(&str[0], str.size(), m_position);
    return str;
}

/**
     * @brief 将字节数组内容转换为十六进制字符串
     * @details
     * - 将字节数组内容逐字节转换为十六进制表示。
     * - 每32字节换行以便于阅读。
     * @return 字节数组内容的十六进制字符串表示
     */
std::string ByteArray::toHexString() const {
    std::string str = toString();
    std::stringstream ss;

    for (size_t i = 0; i < str.size(); i++) {
        if (i > 0 && i % 32 == 0) {
            ss << std::endl;
        }
        ss << std::setw(2) << std::setfill('0') << std::hex << (int)(uint8_t)str[i] << " ";
    }

    return ss.str();
}

/**
     * @brief 获取当前可读数据的缓冲区列表
     * @param[out] buffer 存储缓冲区的iovec结构列表
     * @param[in] len 请求读取的最大长度
     * @details
     * - 根据当前读写位置和请求长度，生成对应的缓冲区列表。
     * - 每个缓冲区对应链表中的一个节点。
     * 这里采用std::vector<iovec>的原因：
     *      ByteArray内部使用链表结构存储数据，每个节点是一个固定大小的内存块。
     *      当需要读取的数据跨越多个节点时，无法用单个连续的内存区域来表示，因此需要多个iovec结构来描述这些分散的内存区域。
     * @return 实际可读的字节数
     */
uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffer, uint64_t len) {
    // 调整请求长度，确保不超过实际可读数据大小
    len = len > getReadSize() ? getReadSize() : len;
    // 如果没有可读数据，直接返回0
    if (len == 0) {
        return 0;
    }

    uint64_t size = len;

    size_t npos = m_position % m_base_size;
    size_t ncap = m_cur_node->size - npos;
    struct iovec iov;
    Node* cur = m_cur_node;

    while (len > 0) {
        if (ncap >= len) {
            // 当前节点可以容纳剩余所有数据
            iov.iov_base = cur->ptr + npos;  // 指向当前节点中数据的起始位置
            iov.iov_len = len;               // 数据长度
            len = 0;
        } else {
            // 当前节点无法容纳所有数据，先处理当前节点的数据
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = ncap;
            len -= ncap;
            cur = cur->next;  // 移动到下一个节点
            ncap = cur->size;
            npos = 0;
        }
        buffer.push_back(iov);  // 将iovec添加到结果数组中
    }
    return size;
}

/**
     * @brief 获取指定位置的可读数据缓冲区列表
     * @param[out] buffer 存储缓冲区的iovec结构列表
     * @param[in] len 请求读取的最大长度
     * @param[in] position 起始读取位置
     * @details
     * - 从指定位置开始，生成对应的缓冲区列表。
     * - 不影响当前读写位置。
     * @return 实际可读的字节数
     */
uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffer, uint64_t len,
                                   uint64_t position) const {
    len = len > getReadSize() ? getReadSize() : len;
    if (len == 0) {
        return 0;
    }

    uint64_t size = len;

    size_t npos = position % m_base_size;
    size_t count = position / m_base_size;
    Node* cur = m_root_node;
    while (count > 0) {
        cur = cur->next;
        --count;
    }

    size_t ncap = cur->size - npos;
    struct iovec iov;
    while (len > 0) {
        if (ncap >= len) {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = len;
            len = 0;
        } else {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = ncap;
            len -= ncap;
            cur = cur->next;
            ncap = cur->size;
            npos = 0;
        }
        buffer.push_back(iov);
    }
    return size;
}

/**
     * @brief 获取当前可写数据的缓冲区列表
     * @param[out] buffer 存储缓冲区的iovec结构列表
     * @param[in] len 请求写入的最大长度
     * @details
     * - 确保有足够的容量存储数据，如果不足则动态扩容。
     * - 根据当前写入位置和请求长度，生成对应的缓冲区列表。
     * @return 实际可写的字节数
     */
uint64_t ByteArray::getWriteBuffers(std::vector<iovec>& buffer, uint64_t len) {
    if (len == 0) {
        return 0;
    }
    addCapacity(len);
    uint64_t size = len;

    size_t npos = m_position % m_base_size;
    size_t ncap = m_cur_node->size - npos;
    struct iovec iov;
    Node* cur = m_cur_node;
    while (len > 0) {
        if (ncap >= len) {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = len;
            len = 0;
        } else {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = ncap;

            len -= ncap;
            cur = cur->next;
            ncap = cur->size;
            npos = 0;
        }
        buffer.push_back(iov);
    }
    return size;
}

/**
     * @brief 动态增加字节数组的容量
     * @param[in] size 请求增加的容量大小
     * @details
     * - 如果剩余容量不足以满足请求，则分配新的节点并连接到链表末尾。
     * - 每个新节点的大小为基础块大小（m_base_size）。
     */
void ByteArray::addCapacity(size_t size) {
    // 如果请求增加的容量为0，则无需扩容
    if (size == 0) {
        return;
    }

    // 获取剩余容量
    size_t old_size = getRemainingCapacity();
    if (old_size >= size) {
        return;
    }

    // 计算实际需要增加的容量大小和所需的内存块数量
    size = size - old_size;
    size_t count = (size / m_base_size) + (((size % m_base_size) > 0) ? 1 : 0);

    // 找到链表中的最后一个节点
    Node* last = m_root_node;
    while (last->next) {
        last = last->next;
    }

    // 创建新的节点并连接到链表末尾
    Node* first = nullptr;
    for (size_t i = 0; i < count; i++) {
        last->next = new Node(m_base_size);
        if (first == nullptr)  // 第一次循环
        {
            first = last->next;
        }
        last = last->next;
        m_capacity += m_base_size;
    }

    // 如果原来没有容量（即这是第一次分配），更新根节点
    if (old_size == 0) {
        m_cur_node = first;
    }
}

size_t ByteArray::getDataSize() const {
    return m_data_size;
}

size_t ByteArray::getRemainingCapacity() const {
    return m_capacity - m_position;
}

size_t ByteArray::getBaseSize() const {
    return m_base_size;
}

size_t ByteArray::getReadSize() const {
    return m_data_size - m_position;
}

bool ByteArray::isLittleEndian() const {
    return m_endian == CIM_LITTLE_ENDIAN;
}
}  // namespace CIM