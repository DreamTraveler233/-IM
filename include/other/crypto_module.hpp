#pragma once

#include "config/config.hpp"
#include "other/module.hpp"
#include "system/env.hpp"
#include "util/crypto_util.hpp"

namespace CIM {
/**
     * @brief 加解密模块：在启动阶段加载 RSA 密钥，并对外提供统一获取接口。
     *
     * 配置项（system.yaml）：
     * - crypto.rsa_private_key_path: 私钥路径（建议相对 bin/，例如 "config/keys/rsa_private_2048.pem"）
     * - crypto.rsa_public_key_path:  公钥路径（PKCS#1：BEGIN RSA PUBLIC KEY）
     * - crypto.padding:              填充策略（"OAEP" | "PKCS1" | "NOPAD"），默认 OAEP
     */
class CryptoModule : public Module {
   public:
    using ptr = std::shared_ptr<CryptoModule>;

    CryptoModule();
    ~CryptoModule() override = default;

    /**
         * @brief 模块生命周期函数：在模块加载时加载RSA密钥
         * @return 加载成功返回true，失败返回false（会导致程序启动中止）
         */
    bool onLoad() override;

    // 访问接口
    /**
         * @brief 获取RSA加密器实例
         * @return RSACipher指针
         */
    CIM::RSACipher::ptr getRSACipher() const { return m_rsa; }

    /**
         * @brief 获取当前使用的填充模式
         * @return 填充模式（如RSA_PKCS1_OAEP_PADDING等）
         */
    int getPadding() const { return m_padding; }

    /**
         * @brief 检查模块是否已就绪（RSA密钥是否加载成功）
         * @return 就绪返回true，否则返回false
         */
    bool isReady() const { return (bool)m_rsa; }

    /**
         * @brief 公钥加密数据
         * @param plaintext[in] 明文数据
         * @param ciphertext[out] 加密后的密文数据
         * @return 加密成功返回true，失败返回false
         */
    bool PublicEncrypt(const std::string& plaintext, std::string& ciphertext) const;

    /**
         * @brief 私钥解密数据
         * @param ciphertext[in] 待解密的密文数据
         * @param plaintext[out] 解密后的明文数据
         * @return 解密成功返回true，失败返回false
         */
    bool PrivateDecrypt(const std::string& ciphertext, std::string& plaintext) const;

    /**
         * @brief 私钥加密数据（通常用于签名）
         * @param input[in] 输入数据
         * @param output[out] 加密后的数据
         * @return 加密成功返回true，失败返回false
         */
    bool PrivateEncrypt(const std::string& input, std::string& output) const;

    /**
         * @brief 公钥解密数据（通常用于验证签名）
         * @param input[in] 待解密的数据
         * @param output[out] 解密后的数据
         * @return 解密成功返回true，失败返回false
         */
    bool PublicDecrypt(const std::string& input, std::string& output) const;

    /**
         * @brief 计算在当前padding模式下的最大明文长度（字节）
         * @return 最大明文长度，失败返回-1
         * @note OAEP按SHA-1估算为k-42；PKCS1为k-11；NOPAD为k（需等长）
         */
    int maxPlaintextLen() const;

    /**
         * @brief 通过ModuleMgr查找当前模块实例
         * @return CryptoModule实例指针
         */
    static CryptoModule::ptr Get();

   private:
    /**
         * @brief 解析填充模式字符串
         * @param name[in] 填充模式名称（如"OAEP"、"PKCS1"等）
         * @return 对应的OpenSSL填充模式常量
         */
    static int ParsePadding(const std::string& name);

    /**
         * @brief 构造绝对路径
         * @param p[in] 原始路径（可能是相对路径）
         * @return 绝对路径
         */
    static std::string MakeAbsPath(const std::string& p);

   private:
    CIM::RSACipher::ptr m_rsa;
    int m_padding = RSA_PKCS1_OAEP_PADDING;  // 默认 OAEP
    mutable RWMutex m_mutex;                 // 简单并发保护
};
}  // namespace CIM