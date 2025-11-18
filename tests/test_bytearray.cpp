#include "base/macro.hpp"
#include "byte_array.hpp"
#include <vector>
#include <iostream>
#include <fstream>

static auto g_logger = CIM_LOG_ROOT();

void test_basic_types()
{
    CIM_LOG_INFO(g_logger) << "Test basic types";

#define XX(type, len, write_fun, read_fun, base_len)                         \
    {                                                                        \
        std::vector<type> vec;                                               \
        for (int i = 0; i < len; ++i)                                        \
        {                                                                    \
            vec.push_back(rand());                                           \
        }                                                                    \
        CIM::ByteArray::ptr ba(new CIM::ByteArray(base_len));            \
        for (auto &it : vec)                                                 \
        {                                                                    \
            ba->write_fun(it);                                               \
        }                                                                    \
        ba->setPosition(0);                                                  \
        for (size_t i = 0; i < vec.size(); ++i)                              \
        {                                                                    \
            type v = ba->read_fun();                                         \
            CIM_ASSERT(v == vec[i]);                                       \
        }                                                                    \
        CIM_ASSERT(ba->getReadSize() == 0);                                \
        CIM_LOG_INFO(g_logger) << #write_fun "/" #read_fun " (" #type ") " \
                                 << len << " base_len=" << base_len          \
                                 << " size=" << ba->getDataSize();           \
    }

    XX(int8_t, 100, writeFint8, readFint8, 1);
    XX(int8_t, 100, writeFuint8, readFuint8, 1);
    XX(int16_t, 100, writeFint16, readFint16, 1);
    XX(int16_t, 100, writeFuint16, readFuint16, 1);
    XX(int32_t, 100, writeFint32, readFint32, 1);
    XX(int32_t, 100, writeFuint32, readFuint32, 1);
    XX(int64_t, 100, writeFint64, readFint64, 1);
    XX(int64_t, 100, writeFuint64, readFuint64, 1);

    XX(int32_t, 100, writeInt32, readInt32, 1);
    XX(int32_t, 100, writeUint32, readUint32, 1);
    XX(int64_t, 100, writeInt64, readInt64, 1);
    XX(int64_t, 100, writeUint64, readUint64, 1);

#undef XX
}

void test_float_types()
{
    CIM_LOG_INFO(g_logger) << "Test float types";

    CIM::ByteArray::ptr ba(new CIM::ByteArray(1));

    // Test float
    float f = 3.1415926f;
    ba->writeFloat(f);
    ba->setPosition(0);
    float f2 = ba->readFloat();
    CIM_ASSERT(f == f2);

    // Test double
    double d = 3.141592653589793;
    ba->writeDouble(d);
    ba->setPosition(sizeof(f));
    double d2 = ba->readDouble();
    CIM_ASSERT(d == d2);

    CIM_LOG_INFO(g_logger) << "Float types test passed";
}

void test_string_types()
{
    CIM_LOG_INFO(g_logger) << "Test string types";

    CIM::ByteArray::ptr ba(new CIM::ByteArray(32));
    std::string str = "Hello, World! 你好世界！";

    // Test F16 string
    ba->writeStringF16(str);
    ba->setPosition(0);
    std::string str1 = ba->readString16();
    CIM_ASSERT(str == str1);
    size_t pos = ba->getPosition();

    // Test F32 string
    ba->writeStringF32(str);
    ba->setPosition(pos);
    std::string str2 = ba->readString32();
    CIM_ASSERT(str == str2);
    pos = ba->getPosition();

    // Test F64 string
    ba->writeStringF64(str);
    ba->setPosition(pos);
    std::string str3 = ba->readString64();
    CIM_ASSERT(str == str3);
    pos = ba->getPosition();

    // Test Vint string
    ba->writeStringVint(str);
    ba->setPosition(pos);
    std::string str4 = ba->readStringVint();
    CIM_ASSERT(str == str4);
    pos = ba->getPosition();

    // Test string without length
    ba->writeStringWithoutLength(str);
    ba->setPosition(pos);
    std::string str5;
    str5.resize(str.length());
    ba->read(&str5[0], str.length());
    CIM_ASSERT(str == str5);

    CIM_LOG_INFO(g_logger) << "String types test passed";
}

void test_file_operations()
{
    CIM_LOG_INFO(g_logger) << "Test file operations";

#define XX(type, len, write_fun, read_fun, base_len)                                                           \
    {                                                                                                          \
        std::vector<type> vec;                                                                                 \
        for (int i = 0; i < len; ++i)                                                                          \
        {                                                                                                      \
            vec.push_back(rand());                                                                             \
        }                                                                                                      \
        CIM::ByteArray::ptr ba(new CIM::ByteArray(base_len));                                              \
        for (auto &it : vec)                                                                                   \
        {                                                                                                      \
            ba->write_fun(it);                                                                                 \
        }                                                                                                      \
        ba->setPosition(0);                                                                                    \
        for (size_t i = 0; i < vec.size(); ++i)                                                                \
        {                                                                                                      \
            type v = ba->read_fun();                                                                           \
            CIM_ASSERT(v == vec[i]);                                                                         \
        }                                                                                                      \
        CIM_ASSERT(ba->getReadSize() == 0);                                                                  \
        ba->setPosition(0);                                                                                    \
        CIM_ASSERT(ba->writeToFile("/home/szy/code/CIM/bin/log/" #type "_" #len "-" #read_fun ".data"));   \
        CIM::ByteArray::ptr ba2(new CIM::ByteArray(base_len * 2));                                         \
        CIM_ASSERT(ba2->readFromFile("/home/szy/code/CIM/bin/log/" #type "_" #len "-" #read_fun ".data")); \
        ba2->setPosition(0);                                                                                   \
        CIM_ASSERT(ba->toString() == ba2->toString());                                                       \
        CIM_ASSERT(ba->getPosition() == 0);                                                                  \
        CIM_ASSERT(ba2->getPosition() == 0);                                                                 \
    }

    XX(int8_t, 100, writeFint8, readFint8, 1);
    XX(int8_t, 100, writeFuint8, readFuint8, 1);
    XX(int16_t, 100, writeFint16, readFint16, 1);
    XX(int16_t, 100, writeFuint16, readFuint16, 1);
    XX(int32_t, 100, writeFint32, readFint32, 1);
    XX(int32_t, 100, writeFuint32, readFuint32, 1);
    XX(int64_t, 100, writeFint64, readFint64, 1);
    XX(int64_t, 100, writeFuint64, readFuint64, 1);

    XX(int32_t, 100, writeInt32, readInt32, 1);
    XX(int32_t, 100, writeUint32, readUint32, 1);
    XX(int64_t, 100, writeInt64, readInt64, 1);
    XX(int64_t, 100, writeUint64, readUint64, 1);

#undef XX

    CIM_LOG_INFO(g_logger) << "file operations test passed";
}

void test_buffer_operations()
{
    CIM_LOG_INFO(g_logger) << "Test buffer operations";

    CIM::ByteArray::ptr ba(new CIM::ByteArray(16));

    // Write some data
    for (int i = 0; i < 100; ++i)
    {
        ba->writeFint32(i);
    }

    ba->setPosition(0);

    // Test getReadBuffers
    std::vector<iovec> read_buffers;
    uint64_t read_len = ba->getReadBuffers(read_buffers, 400);
    CIM_ASSERT(read_len == 400);
    CIM_ASSERT(!read_buffers.empty());

    // Test getWriteBuffers
    std::vector<iovec> write_buffers;
    uint64_t write_len = ba->getWriteBuffers(write_buffers, 500);
    CIM_ASSERT(write_len == 500);
    CIM_ASSERT(!write_buffers.empty());

    CIM_LOG_INFO(g_logger) << "Buffer operations test passed";
}

void test_edge_cases()
{
    CIM_LOG_INFO(g_logger) << "Test edge cases";

    CIM::ByteArray::ptr ba(new CIM::ByteArray(1));

    // Test empty string
    ba->writeStringVint("");
    ba->setPosition(0);
    std::string empty = ba->readStringVint();
    CIM_ASSERT(empty.empty());

    size_t pos = ba->getPosition();

    // Test zero values
    ba->writeFint32(0);
    ba->setPosition(pos);
    CIM_ASSERT(ba->readFint32() == 0);
    pos = ba->getPosition();

    ba->writeFuint64(0);
    ba->setPosition(pos);
    CIM_ASSERT(ba->readFuint64() == 0);
    pos = ba->getPosition();

    ba->writeInt32(0);
    ba->setPosition(pos);
    CIM_ASSERT(ba->readInt32() == 0);
    pos = ba->getPosition();

    ba->writeUint64(0);
    ba->setPosition(pos);
    CIM_ASSERT(ba->readUint64() == 0);
    pos = ba->getPosition();

    // Test negative values
    ba->writeFint32(-1);
    ba->setPosition(pos);
    CIM_ASSERT(ba->readFint32() == -1);
    pos = ba->getPosition();

    ba->writeInt32(-1);
    ba->setPosition(pos);
    CIM_ASSERT(ba->readInt32() == -1);
    pos = ba->getPosition();

    ba->writeInt64(-10000000000LL);
    ba->setPosition(pos);
    CIM_ASSERT(ba->readInt64() == -10000000000LL);
    pos = ba->getPosition();

    // Test large values
    ba->writeFuint64(UINT64_MAX);
    ba->setPosition(pos);
    CIM_ASSERT(ba->readFuint64() == UINT64_MAX);
    pos = ba->getPosition();

    ba->writeUint64(UINT64_MAX);
    ba->setPosition(pos);
    CIM_ASSERT(ba->readUint64() == UINT64_MAX);

    CIM_LOG_INFO(g_logger) << "Edge cases test passed";
}

void test_byte_order()
{
    CIM_LOG_INFO(g_logger) << "Test byte order";

    CIM::ByteArray::ptr ba(new CIM::ByteArray(1));

    // Test big endian (default)
    CIM_ASSERT(!ba->isLittleEndian());

    // Test little endian
    ba->setIsLittleEndian(true);
    CIM_ASSERT(ba->isLittleEndian());

    ba->writeFint32(0x12345678);
    ba->setPosition(0);
    int32_t value = ba->readFint32();
    CIM_ASSERT(value == 0x12345678);

    CIM_LOG_INFO(g_logger) << "Byte order test passed";
}

void test_clear_and_positions()
{
    CIM_LOG_INFO(g_logger) << "Test clear and positions";

    CIM::ByteArray::ptr ba(new CIM::ByteArray(16));

    // Add some data
    for (int i = 0; i < 10; ++i)
    {
        ba->writeFint32(i);
    }

    CIM_ASSERT(ba->getDataSize() == 40);
    CIM_ASSERT(ba->getPosition() == 40);
    CIM_ASSERT(ba->getReadSize() == 0);

    // Test setPosition
    ba->setPosition(8);
    CIM_ASSERT(ba->getPosition() == 8);
    CIM_ASSERT(ba->getReadSize() == 32);

    // Test clear
    ba->clear();
    CIM_ASSERT(ba->getDataSize() == 0);
    CIM_ASSERT(ba->getPosition() == 0);
    CIM_ASSERT(ba->getReadSize() == 0);

    CIM_LOG_INFO(g_logger) << "Clear and positions test passed";
}

void test_to_string_functions()
{
    CIM_LOG_INFO(g_logger) << "Test to string functions";

    CIM::ByteArray::ptr ba(new CIM::ByteArray(16));
    std::string test_str = "ByteArray to string test";

    ba->writeStringWithoutLength(test_str);
    ba->setPosition(0);

    std::string str_result = ba->toString();
    CIM_ASSERT(str_result == test_str);

    std::string hex_result = ba->toHexString();
    CIM_ASSERT(!hex_result.empty());

    CIM_LOG_INFO(g_logger) << "To string functions test passed";
}

int main(int argc, char **argv)
{
    srand(time(NULL));

    try
    {
        test_basic_types();
        test_float_types();
        test_string_types();
        test_file_operations();
        test_buffer_operations();
        test_edge_cases();
        test_byte_order();
        test_clear_and_positions();
        test_to_string_functions();

        CIM_LOG_INFO(g_logger) << "All tests passed!";
    }
    catch (...)
    {
        CIM_LOG_ERROR(g_logger) << "Test failed!";
        return 1;
    }

    return 0;
}