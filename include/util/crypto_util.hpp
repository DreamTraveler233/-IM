#ifndef __CIM_UTIL_CRYPTO_UTIL_HPP__
#define __CIM_UTIL_CRYPTO_UTIL_HPP__

/**
 * @file crypto_util.hpp
 * @brief 加解密实用工具封装（基于 OpenSSL）。
 *
 * 功能概述：
 * - 对称加密：AES-128/256，ECB 与 CBC 模式；
 * - 非对称加密：RSA（公私钥加解密，支持原始/可选填充）。
 *
 * 依赖：OpenSSL（<openssl/ssl.h>, <openssl/evp.h>）。
 * 线程安全：本文件提供的静态函数本身为无状态；但 RSA
 * 对象（RSACipher）不是多线程复用安全的， 建议每个线程持有独立实例或自行加锁。
 *
 * 安全注意：
 * - 默认 RSA 使用
 * RSA_NO_PADDING（无填充），极易产生安全风险，除非有明确协议需求， 强烈建议使用
 * RSA_PKCS1_PADDING 或 RSA_PKCS1_OAEP_PADDING。
 * - ECB 模式具备模式泄露风险，生产环境优先选用 CBC/GCM 等模式，并正确管理
 * IV/Nonce。
 * - out 缓冲区需预留足够长度：对称加密通常为 in_len + block_size（考虑填充）；
 *   RSA 相关缓冲至少为 RSA_size(key)。
 */

#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <stdint.h>

#include <memory>
#include <string>

namespace CIM {
/**
     * @brief 对称加密工具（AES 封装）。
     */
class CryptoUtil {
   public:
    /**
         * @brief AES-256-ECB 加/解密。
         *
         * @param key     密钥指针（长度必须为 32 字节）。
         * @param in      输入明文/密文缓冲。
         * @param in_len  输入长度（字节）。
         * @param out     输出缓冲，需由调用方分配；建议容量 >= in_len + 16。
         * @param encode  true=加密, false=解密。
         * @return int32_t 实际输出字节数，<0 表示失败。
         *
         * 注意：ECB 无 IV；ECB 不具备语义安全，谨慎使用。
         */
    static int32_t AES256Ecb(const void* key, const void* in, int32_t in_len, void* out,
                             bool encode);

    /**
         * @brief AES-128-ECB 加/解密。
         *
         * @param key     密钥指针（长度必须为 16 字节）。
         * @param in      输入明文/密文缓冲。
         * @param in_len  输入长度（字节）。
         * @param out     输出缓冲，需由调用方分配；建议容量 >= in_len + 16。
         * @param encode  true=加密, false=解密。
         * @return int32_t 实际输出字节数，<0 表示失败。
         */
    static int32_t AES128Ecb(const void* key, const void* in, int32_t in_len, void* out,
                             bool encode);

    /**
         * @brief AES-256-CBC 加/解密。
         *
         * @param key     密钥指针（长度必须为 32 字节）。
         * @param iv      初始向量（IV）指针（长度必须为 16 字节）。
         * @param in      输入明文/密文缓冲。
         * @param in_len  输入长度（字节）。
         * @param out     输出缓冲，需由调用方分配；建议容量 >= in_len + 16。
         * @param encode  true=加密, false=解密。
         * @return int32_t 实际输出字节数，<0 表示失败。
         *
         * 注意：IV 必须唯一且不可复用于同一密钥下的不同消息。
         */
    static int32_t AES256Cbc(const void* key, const void* iv, const void* in, int32_t in_len,
                             void* out, bool encode);

    /**
         * @brief AES-128-CBC 加/解密。
         *
         * @param key     密钥指针（长度必须为 16 字节）。
         * @param iv      初始向量（IV）指针（长度必须为 16 字节）。
         * @param in      输入明文/密文缓冲。
         * @param in_len  输入长度（字节）。
         * @param out     输出缓冲，需由调用方分配；建议容量 >= in_len + 16。
         * @param encode  true=加密, false=解密。
         * @return int32_t 实际输出字节数，<0 表示失败。
         */
    static int32_t AES128Cbc(const void* key, const void* iv, const void* in, int32_t in_len,
                             void* out, bool encode);

    /**
         * @brief 通用 EVP_CIPHER 加/解密封装。
         *
         * @param cipher  OpenSSL EVP_CIPHER 算法指针（如 EVP_aes_256_cbc()）。
         * @param enc     true=加密, false=解密。
         * @param key     密钥指针（长度需与算法匹配）。
         * @param iv      IV 指针（若算法需要，长度需与算法匹配；不需要可为
         * nullptr）。
         * @param in      输入缓冲（明/密文）。
         * @param in_len  输入长度（字节）。
         * @param out     输出缓冲（由调用方分配）。
         * @param out_len 输出实际字节数（返回时写入）。
         * @return int32_t 0 表示成功，非 0 表示失败。
         *
         * 备注：调用方需保证 out 的容量充足（通常 >= in_len + block_size）。
         */
    static int32_t Crypto(const EVP_CIPHER* cipher, bool enc, const void* key, const void* iv,
                          const void* in, int32_t in_len, void* out, int32_t* out_len);
};

/**
     * @brief RSA 加解密封装。
     *
     * 用途：基于文件读取或生成 RSA
     * 公私钥，提供公钥加密/私钥解密以及私钥加密/公钥解密接口。
     *
     * 安全提示：默认 padding=RSA_NO_PADDING，实际使用中请优先选择
     *           RSA_PKCS1_PADDING 或 RSA_PKCS1_OAEP_PADDING。
     */
class RSACipher {
   public:
    typedef std::shared_ptr<RSACipher> ptr;

    /**
         * @brief 生成 RSA 密钥对并保存到文件。
         *
         * @param pubkey_file 公钥输出文件路径（PEM 格式）。
         * @param prikey_file 私钥输出文件路径（PEM 格式）。
         * @param length      密钥长度（位），默认 1024，建议 >= 2048。
         * @return int32_t 0 表示成功，非 0 表示失败。
         */
    static int32_t GenerateKey(const std::string& pubkey_file, const std::string& prikey_file,
                               uint32_t length = 1024);

    /**
         * @brief 从公私钥文件创建 RSACipher 实例。
         *
         * @param pubkey_file 公钥文件路径（PEM）。可为空字符串表示仅加载私钥。
         * @param prikey_file 私钥文件路径（PEM）。可为空字符串表示仅加载公钥。
         * @return RSACipher::ptr 成功返回非空指针，失败返回 nullptr。
         */
    static RSACipher::ptr Create(const std::string& pubkey_file, const std::string& prikey_file);

    /** @brief 构造与析构：负责内部 RSA 资源的管理与释放。 */
    RSACipher();
    ~RSACipher();

    /**
         * @brief 使用私钥加密（通常用于签名等场景）。
         * @param from    输入缓冲。
         * @param flen    输入长度（字节）。
         * @param to      输出缓冲，容量需 >= getPriRSASize()。
         * @param padding 填充方式，默认
         * RSA_NO_PADDING（不安全，仅在协议需要时使用）。
         * @return int32_t 成功返回输出字节数，失败返回 <0。
         */
    int32_t privateEncrypt(const void* from, int flen, void* to, int padding = RSA_NO_PADDING);
    /**
         * @brief 使用公钥加密（典型加密场景）。
         * @param from    输入缓冲。
         * @param flen    输入长度（字节）。
         * @param to      输出缓冲，容量需 >= getPubRSASize()。
         * @param padding 填充方式，建议 RSA_PKCS1_PADDING 或 OAEP。
         * @return int32_t 成功返回输出字节数，失败返回 <0。
         */
    int32_t publicEncrypt(const void* from, int flen, void* to, int padding = RSA_NO_PADDING);
    /**
         * @brief 使用私钥解密（与 publicEncrypt 对应）。
         * @param from    输入密文。
         * @param flen    输入长度（字节）。
         * @param to      输出缓冲，容量需 >= getPriRSASize()。
         * @param padding 填充方式，需与加密端一致。
         * @return int32_t 成功返回输出字节数，失败返回 <0。
         */
    int32_t privateDecrypt(const void* from, int flen, void* to, int padding = RSA_NO_PADDING);
    /**
         * @brief 使用公钥解密（与 privateEncrypt 对应，常见于验签流程中的原始恢复）。
         * @param from    输入密文。
         * @param flen    输入长度（字节）。
         * @param to      输出缓冲，容量需 >= getPubRSASize()。
         * @param padding 填充方式，需与加密端一致。
         * @return int32_t 成功返回输出字节数，失败返回 <0。
         */
    int32_t publicDecrypt(const void* from, int flen, void* to, int padding = RSA_NO_PADDING);
    /**
         * @brief 私钥加密，输出到 std::string。
         * @return int32_t 成功返回输出字节数，失败返回 <0。
         */
    int32_t privateEncrypt(const void* from, int flen, std::string& to,
                           int padding = RSA_NO_PADDING);
    /**
         * @brief 公钥加密，输出到 std::string。
         * @return int32_t 成功返回输出字节数，失败返回 <0。
         */
    int32_t publicEncrypt(const void* from, int flen, std::string& to,
                          int padding = RSA_NO_PADDING);
    /**
         * @brief 私钥解密，输出到 std::string。
         * @return int32_t 成功返回输出字节数，失败返回 <0。
         */
    int32_t privateDecrypt(const void* from, int flen, std::string& to,
                           int padding = RSA_NO_PADDING);
    /**
         * @brief 公钥解密，输出到 std::string。
         * @return int32_t 成功返回输出字节数，失败返回 <0。
         */
    int32_t publicDecrypt(const void* from, int flen, std::string& to,
                          int padding = RSA_NO_PADDING);

    /** @brief 获取加载的公钥 PEM 内容（原始字符串）。
         *
         */
    const std::string& getPubkeyStr() const { return m_pubkeyStr; }
    /** @brief 获取加载的私钥 PEM 内容（原始字符串）。
         *
         */
    const std::string& getPrikeyStr() const { return m_prikeyStr; }

    /**
         * @brief 获取公钥模数尺寸（字节），等价于 RSA_size(pubkey)。
         * @return int32_t >0 返回尺寸，<=0 表示失败或未加载公钥。
         */
    int32_t getPubRSASize();
    /**
         * @brief 获取私钥模数尺寸（字节），等价于 RSA_size(prikey)。
         * @return int32_t >0 返回尺寸，<=0 表示失败或未加载私钥。
         */
    int32_t getPriRSASize();

   private:
    /** @brief OpenSSL RSA* 原始句柄（公钥/私钥）。 */
    RSA* m_pubkey;
    RSA* m_prikey;
    /** @brief PEM 格式原文保存（可为空）。 */
    std::string m_pubkeyStr;
    std::string m_prikeyStr;
};

}  // namespace CIM

#endif