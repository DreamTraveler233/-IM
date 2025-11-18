#ifndef __CIM_UTIL_UTIL_CPP__
#define __CIM_UTIL_UTIL_CPP__

#include <cxxabi.h>
#include <jsoncpp/json/json.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <yaml-cpp/yaml.h>

#include <boost/lexical_cast.hpp>
#include <fstream>
#include <string>
#include <vector>

#include "crypto_util.hpp"
#include "hash_util.hpp"
#include "json_util.hpp"
#include "string_util.hpp"
#include "time_util.hpp"

namespace CIM {
/**
     * @brief 获取当前线程的真实线程ID
     * @return 当前线程的系统线程ID（TID）
     *
     * 该函数通过系统调用获取当前线程的真实线程ID，
     * 与pthread_self()返回的pthread_t不同，该ID是系统级别的线程标识符。
     * 主要用于日志记录、调试和线程识别等场景。
     */
pid_t GetThreadId();

/**
     * @brief 获取当前协程ID
     * @return 当前协程的唯一标识符
     *
     * 该函数返回当前正在运行的协程ID。
     * 在协程编程中用于识别和跟踪不同的协程执行单元。
     * 如果当前不在协程环境中，可能返回特定的默认值（如0表示主线程）。
     */
uint64_t GetCoroutineId();

/**
     * @brief 获取函数调用堆栈信息
     * @param bt[out] 存储堆栈信息的字符串向量
     * @param size[in] 需要获取的堆栈层数
     * @param skip[in] 跳过的堆栈层数，从第skip层开始记录
     */
void Backtrace(std::vector<std::string>& bt, int size = 64, int skip = 1);

/**
     * @brief 将函数调用堆栈信息转换为字符串
     * @param size 需要获取的堆栈层数
     * @param skip 跳过的堆栈层数，从第skip层开始记录
     * @param prefix 每行输出的前缀字符串
     * @return 包含堆栈信息的字符串
     */
std::string BacktraceToString(int size = 64, int skip = 2, const std::string& prefix = "    ");

template <class T>
const char* TypeToName() {
    // 确保永远不返回空指针，避免在构造 std::string 或输出时触发 undefined behavior
    static const char* s_name = []() -> const char* {
        int status = 0;
        char* demangled = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, &status);
        return demangled ? demangled : typeid(T).name();
    }();
    return s_name;
}

class FSUtil {
   public:
    /**
         * @brief 递归列举指定目录下满足后缀条件的全部文件
         * @param files[out] 文件路径收集容器
         * @param path[in] 需要遍历的起始目录
         * @param subfix[in] 文件名后缀过滤条件，空字符串表示不过滤
         */
    static void ListAllFile(std::vector<std::string>& files, const std::string& path,
                            const std::string& subfix);

    /**
         * @brief 创建目录，自动创建缺失的父级目录
         * @param dirname[in] 目标目录路径
         * @return 创建成功返回true，目录已存在也返回true
         */
    static bool Mkdir(const std::string& dirname);

    /**
         * @brief 检查PID文件对应的进程是否仍在运行
         * @param pidfile[in] PID文件路径
         * @return 进程仍存在返回true
         */
    static bool IsRunningPidfile(const std::string& pidfile);

    /**
         * @brief 删除文件或目录
         * @param path[in] 目标路径
         * @return 删除成功返回true
         */
    static bool Rm(const std::string& path);

    /**
         * @brief 重命名或移动文件/目录
         * @param from[in] 原始路径
         * @param to[in] 新路径
         */
    static bool Mv(const std::string& from, const std::string& to);

    /**
         * @brief 获取路径的真实绝对路径
         * @param path[in] 原始路径
         * @param rpath[out] 解析后的绝对路径
         */
    static bool Realpath(const std::string& path, std::string& rpath);

    /**
         * @brief 创建符号链接
         * @param frm[in] 源文件路径
         * @param to[in] 符号链接目标路径
         */
    static bool Symlink(const std::string& frm, const std::string& to);

    /**
         * @brief 删除文件，可选忽略不存在情况
         * @param filename[in] 文件路径
         * @param exist[in] 为true时仅当文件存在才删除
         */
    static bool Unlink(const std::string& filename, bool exist = false);

    /**
         * @brief 提取路径中的目录部分
         */
    static std::string Dirname(const std::string& filename);

    /**
         * @brief 提取路径中的文件名部分
         */
    static std::string Basename(const std::string& filename);

    /**
         * @brief 以读取方式打开文件流
         * @param ifs[out] 打开的输入文件流
         * @param filename[in] 文件路径
         * @param mode[in] 打开模式，默认为std::ios::in
         */
    static bool OpenForRead(std::ifstream& ifs, const std::string& filename,
                            std::ios_base::openmode mode);

    /**
         * @brief 以写入方式打开文件流
         * @param ofs[out] 打开的输出文件流
         * @param filename[in] 文件路径
         * @param mode[in] 打开模式，默认为std::ios::out
         */
    static bool OpenForWrite(std::ofstream& ofs, const std::string& filename,
                             std::ios_base::openmode mode);
};

template <class V, class Map, class K>
/**
     * @brief 从映射容器中读取参数值
     * @param m[in] 键值容器
     * @param k[in] 目标键
     * @param def[in] 当不存在或转换失败时的默认值
     */
V GetParamValue(const Map& m, const K& k, const V& def = V()) {
    auto it = m.find(k);
    if (it == m.end()) {
        return def;
    }
    try {
        return boost::lexical_cast<V>(it->second);
    } catch (...) {
    }
    return def;
}

template <class V, class Map, class K>
/**
     * @brief 从映射容器中读取参数值并指示是否成功
     * @param m[in] 键值容器
     * @param k[in] 目标键
     * @param v[out] 转换后的目标值
     * @return 成功返回true，失败返回false
     */
bool CheckGetParamValue(const Map& m, const K& k, V& v) {
    auto it = m.find(k);
    if (it == m.end()) {
        return false;
    }
    try {
        v = boost::lexical_cast<V>(it->second);
        return true;
    } catch (...) {
    }
    return false;
}

/**
     * @brief 将YAML节点转换为JSON节点
     */
bool YamlToJson(const YAML::Node& ynode, Json::Value& jnode);

/**
     * @brief 将JSON节点转换为YAML节点
     */
bool JsonToYaml(const Json::Value& jnode, YAML::Node& ynode);

/**
     * @brief 获取当前主机名
     */
std::string GetHostName();

/**
     * @brief 获取本机IPv4地址
     */
std::string GetIPv4();

template <class T>
/**
     * @brief 空操作函数，用于模板占位
     */
void nop(T*) {}

template <class T>
/**
     * @brief 释放数组指针
     */
void delete_array(T* v) {
    if (v) {
        delete[] v;
    }
}

/**
     * @brief 字符串转大写
     */
std::string ToUpper(const std::string& name);

/**
     * @brief 字符串转小写
     */
std::string ToLower(const std::string& name);

class TypeUtil {
   public:
    /**
         * @brief 字符串转换为字符
         */
    static int8_t ToChar(const std::string& str);

    /**
         * @brief 字符串转换为64位整数
         */
    static int64_t Atoi(const std::string& str);

    /**
         * @brief 字符串转换为浮点数
         */
    static double Atof(const std::string& str);

    /**
         * @brief C字符串转换为字符
         */
    static int8_t ToChar(const char* str);

    /**
         * @brief C字符串转换为64位整数
         */
    static int64_t Atoi(const char* str);

    /**
         * @brief C字符串转换为浮点数
         */
    static double Atof(const char* str);
};

class Atomic {
   public:
    /**
         * @brief 原子加并返回结果
         */
    template <class T, class S = T>
    static T addFetch(volatile T& t, S v = 1) {
        return __sync_add_and_fetch(&t, (T)v);
    }

    /**
         * @brief 原子减并返回结果
         */
    template <class T, class S = T>
    static T subFetch(volatile T& t, S v = 1) {
        return __sync_sub_and_fetch(&t, (T)v);
    }

    /**
         * @brief 原子或并返回结果
         */
    template <class T, class S>
    static T orFetch(volatile T& t, S v) {
        return __sync_or_and_fetch(&t, (T)v);
    }

    /**
         * @brief 原子与并返回结果
         */
    template <class T, class S>
    static T andFetch(volatile T& t, S v) {
        return __sync_and_and_fetch(&t, (T)v);
    }

    /**
         * @brief 原子异或并返回结果
         */
    template <class T, class S>
    static T xorFetch(volatile T& t, S v) {
        return __sync_xor_and_fetch(&t, (T)v);
    }

    /**
         * @brief 原子NAND并返回结果
         */
    template <class T, class S>
    static T nandFetch(volatile T& t, S v) {
        return __sync_nand_and_fetch(&t, (T)v);
    }

    /**
         * @brief 原子加并返回旧值
         */
    template <class T, class S>
    static T fetchAdd(volatile T& t, S v = 1) {
        return __sync_fetch_and_add(&t, (T)v);
    }

    /**
         * @brief 原子减并返回旧值
         */
    template <class T, class S>
    static T fetchSub(volatile T& t, S v = 1) {
        return __sync_fetch_and_sub(&t, (T)v);
    }

    /**
         * @brief 原子或并返回旧值
         */
    template <class T, class S>
    static T fetchOr(volatile T& t, S v) {
        return __sync_fetch_and_or(&t, (T)v);
    }

    /**
         * @brief 原子与并返回旧值
         */
    template <class T, class S>
    static T fetchAnd(volatile T& t, S v) {
        return __sync_fetch_and_and(&t, (T)v);
    }

    /**
         * @brief 原子异或并返回旧值
         */
    template <class T, class S>
    static T fetchXor(volatile T& t, S v) {
        return __sync_fetch_and_xor(&t, (T)v);
    }

    /**
         * @brief 原子NAND并返回旧值
         */
    template <class T, class S>
    static T fetchNand(volatile T& t, S v) {
        return __sync_fetch_and_nand(&t, (T)v);
    }

    /**
         * @brief 带返回旧值的原子比较并交换
         */
    template <class T, class S>
    static T compareAndSwap(volatile T& t, S old_val, S new_val) {
        return __sync_val_compare_and_swap(&t, (T)old_val, (T)new_val);
    }

    /**
         * @brief 原子比较并交换，返回是否成功
         */
    template <class T, class S>
    static bool compareAndSwapBool(volatile T& t, S old_val, S new_val) {
        return __sync_bool_compare_and_swap(&t, (T)old_val, (T)new_val);
    }
};

template <class Iter>
/**
     * @brief 将迭代区间元素使用分隔符拼接为字符串
     */
std::string Join(Iter begin, Iter end, const std::string& tag) {
    std::stringstream ss;
    for (Iter it = begin; it != end; ++it) {
        if (it != begin) {
            ss << tag;
        }
        ss << *it;
    }
    return ss.str();
}

template <class T>
class SharedArray {
   public:
    /**
         * @brief 构造空的共享数组
         * @param size[in] 数组元素数量，仅做元数据记录
         * @param p[in] 原始数组指针
         */
    explicit SharedArray(const uint64_t& size = 0, T* p = 0)
        : m_size(size), m_ptr(p, delete_array<T>) {}

    template <class D>
    /**
         * @brief 使用自定义删除器的构造函数
         */
    SharedArray(const uint64_t& size, T* p, D d) : m_size(size), m_ptr(p, d){};

    /**
         * @brief 拷贝构造，共享底层资源
         */
    SharedArray(const SharedArray& r) : m_size(r.m_size), m_ptr(r.m_ptr) {}

    /**
         * @brief 拷贝赋值，共享底层资源
         */
    SharedArray& operator=(const SharedArray& r) {
        m_size = r.m_size;
        m_ptr = r.m_ptr;
        return *this;
    }

    /**
         * @brief 数组下标访问
         */
    T& operator[](std::ptrdiff_t i) const { return m_ptr.get()[i]; }

    /**
         * @brief 获取原始数组指针
         */
    T* get() const { return m_ptr.get(); }

    /**
         * @brief 判断是否仅有唯一引用
         */
    bool unique() const { return m_ptr.unique(); }

    /**
         * @brief 获取引用计数
         */
    long use_count() const { return m_ptr.use_count(); }

    /**
         * @brief 交换两个共享数组的内容
         */
    void swap(SharedArray& b) {
        std::swap(m_size, b.m_size);
        m_ptr.swap(b.m_ptr);
    }

    /**
         * @brief 判断是否为空
         */
    bool operator!() const { return !m_ptr; }

    /**
         * @brief 判断是否持有有效指针
         */
    operator bool() const { return !!m_ptr; }

    /**
         * @brief 获取数组元素数量
         */
    uint64_t size() const { return m_size; }

   private:
    uint64_t m_size;
    std::shared_ptr<T> m_ptr;
};
}  // namespace CIM

#endif