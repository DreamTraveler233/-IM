#include "util/crypto_util.hpp"

#include <openssl/pem.h>
#include <stdio.h>

#include <iostream>

#include "base/macro.hpp"

// 抑制 OpenSSL 3.0 中 RSA 旧API的弃用告警（该文件内部按需兼容）
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

namespace CIM {
static auto g_logger = CIM_LOG_NAME("util");
int32_t CryptoUtil::AES256Ecb(const void* key, const void* in, int32_t in_len, void* out,
                              bool encode) {
    int32_t len = 0;
    return Crypto(EVP_aes_256_ecb(), encode, (const uint8_t*)key, nullptr, (const uint8_t*)in,
                  in_len, (uint8_t*)out, &len);
}

int32_t CryptoUtil::AES128Ecb(const void* key, const void* in, int32_t in_len, void* out,
                              bool encode) {
    int32_t len = 0;
    return Crypto(EVP_aes_128_ecb(), encode, (const uint8_t*)key, nullptr, (const uint8_t*)in,
                  in_len, (uint8_t*)out, &len);
}

int32_t CryptoUtil::AES256Cbc(const void* key, const void* iv, const void* in, int32_t in_len,
                              void* out, bool encode) {
    int32_t len = 0;
    return Crypto(EVP_aes_256_cbc(), encode, (const uint8_t*)key, (const uint8_t*)iv,
                  (const uint8_t*)in, in_len, (uint8_t*)out, &len);
}

int32_t CryptoUtil::AES128Cbc(const void* key, const void* iv, const void* in, int32_t in_len,
                              void* out, bool encode) {
    int32_t len = 0;
    return Crypto(EVP_aes_128_cbc(), encode, (const uint8_t*)key, (const uint8_t*)iv,
                  (const uint8_t*)in, in_len, (uint8_t*)out, &len);
}

int32_t CryptoUtil::Crypto(const EVP_CIPHER* cipher, bool enc, const void* key, const void* iv,
                           const void* in, int32_t in_len, void* out, int32_t* out_len) {
    int tmp_len = 0;
    bool has_error = false;
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    do {
        if (!ctx) {
            has_error = true;
            break;
        }
        // 初始化上下文
        if (EVP_CipherInit_ex(ctx, cipher, nullptr, nullptr, nullptr, enc) != 1) {
            has_error = true;
            break;
        }
        // 设置 key/iv
        if (EVP_CIPHER_key_length(cipher) > 0 || EVP_CIPHER_iv_length(cipher) > 0) {
            if (EVP_CipherInit_ex(ctx, nullptr, nullptr, (const uint8_t*)key, (const uint8_t*)iv,
                                  enc) != 1) {
                has_error = true;
                break;
            }
        }
        if (EVP_CipherUpdate(ctx, (uint8_t*)out, &tmp_len, (const uint8_t*)in, in_len) != 1) {
            has_error = true;
            break;
        }
        *out_len = tmp_len;
        if (EVP_CipherFinal_ex(ctx, (uint8_t*)out + tmp_len, &tmp_len) != 1) {
            has_error = true;
            break;
        }
        *out_len += tmp_len;
    } while (0);
    if (ctx) {
        EVP_CIPHER_CTX_free(ctx);
    }
    if (has_error) {
        return -1;
    }
    return *out_len;
}

int32_t RSACipher::GenerateKey(const std::string& pubkey_file, const std::string& prikey_file,
                               uint32_t length) {
    int rt = 0;
    FILE* fp = nullptr;
    RSA* rsa = nullptr;
    do {
        rsa = RSA_generate_key(length, RSA_F4, NULL, NULL);
        if (!rsa) {
            rt = -1;
            break;
        }

        fp = fopen(pubkey_file.c_str(), "w+");
        if (!fp) {
            rt = -2;
            break;
        }
        PEM_write_RSAPublicKey(fp, rsa);
        fclose(fp);
        fp = nullptr;

        fp = fopen(prikey_file.c_str(), "w+");
        if (!fp) {
            rt = -3;
            break;
        }
        PEM_write_RSAPrivateKey(fp, rsa, NULL, NULL, 0, NULL, NULL);
        fclose(fp);
        fp = nullptr;
    } while (false);
    if (fp) {
        fclose(fp);
    }
    if (rsa) {
        RSA_free(rsa);
    }
    return rt;
}

RSACipher::ptr RSACipher::Create(const std::string& pubkey_file, const std::string& prikey_file) {
    FILE* fp = nullptr;
    do {
        RSACipher::ptr rt(new RSACipher);
        // 读取公钥：优先 PKCS#1 (BEGIN RSA PUBLIC KEY)，失败回退到 SubjectPublicKeyInfo (BEGIN PUBLIC KEY)
        fp = fopen(pubkey_file.c_str(), "r");
        if (!fp) {
            CIM_LOG_ERROR(g_logger) << "CryptoModule: load RSA public key failed: " << pubkey_file;
            break;
        }
        rt->m_pubkey = PEM_read_RSAPublicKey(fp, NULL, NULL, NULL);
        if (!rt->m_pubkey) {
            rewind(fp);
            EVP_PKEY* pkey = PEM_read_PUBKEY(fp, NULL, NULL, NULL);
            if (pkey) {
                rt->m_pubkey = EVP_PKEY_get1_RSA(pkey);
                EVP_PKEY_free(pkey);
            }
        }
        // 读取公钥原文
        {
            std::string tmp;
            tmp.resize(1024);
            int len = 0;
            rewind(fp);
            do {
                len = fread(&tmp[0], 1, tmp.size(), fp);
                if (len > 0) {
                    rt->m_pubkeyStr.append(tmp.c_str(), len);
                }
            } while (len > 0);
        }
        fclose(fp);
        fp = nullptr;
        if (!rt->m_pubkey) {
            CIM_LOG_ERROR(g_logger) << "CryptoModule: load RSA public key failed: " << pubkey_file;
            break;
        }

        // 读取私钥：优先 PKCS#1 (BEGIN RSA PRIVATE KEY)，失败回退到 PKCS#8 (BEGIN PRIVATE KEY)
        fp = fopen(prikey_file.c_str(), "r");
        if (!fp) {
            break;
        }
        rt->m_prikey = PEM_read_RSAPrivateKey(fp, NULL, NULL, NULL);
        if (!rt->m_prikey) {
            rewind(fp);
            EVP_PKEY* pkey = PEM_read_PrivateKey(fp, NULL, NULL, NULL);
            if (pkey) {
                rt->m_prikey = EVP_PKEY_get1_RSA(pkey);
                EVP_PKEY_free(pkey);
            }
        }
        // 读取私钥原文
        {
            std::string tmp;
            tmp.resize(1024);
            int len = 0;
            rewind(fp);
            do {
                len = fread(&tmp[0], 1, tmp.size(), fp);
                if (len > 0) {
                    rt->m_prikeyStr.append(tmp.c_str(), len);
                }
            } while (len > 0);
        }
        fclose(fp);
        fp = nullptr;
        if (!rt->m_prikey) {
            break;
        }
        return rt;
    } while (false);
    if (fp) {
        fclose(fp);
    }
    return nullptr;
}

RSACipher::RSACipher() : m_pubkey(nullptr), m_prikey(nullptr) {}

RSACipher::~RSACipher() {
    if (m_pubkey) {
        RSA_free(m_pubkey);
    }
    if (m_prikey) {
        RSA_free(m_prikey);
    }
}

int32_t RSACipher::privateEncrypt(const void* from, int flen, void* to, int padding) {
    return RSA_private_encrypt(flen, (const uint8_t*)from, (uint8_t*)to, m_prikey, padding);
}

int32_t RSACipher::publicEncrypt(const void* from, int flen, void* to, int padding) {
    return RSA_public_encrypt(flen, (const uint8_t*)from, (uint8_t*)to, m_pubkey, padding);
}

int32_t RSACipher::privateDecrypt(const void* from, int flen, void* to, int padding) {
    return RSA_private_decrypt(flen, (const uint8_t*)from, (uint8_t*)to, m_prikey, padding);
}

int32_t RSACipher::publicDecrypt(const void* from, int flen, void* to, int padding) {
    return RSA_public_decrypt(flen, (const uint8_t*)from, (uint8_t*)to, m_pubkey, padding);
}

int32_t RSACipher::privateEncrypt(const void* from, int flen, std::string& to, int padding) {
    int rsa_size = getPriRSASize();
    if (rsa_size <= 0) {
        to.clear();
        return -1;
    }
    to.resize(static_cast<size_t>(rsa_size));
    int32_t len = privateEncrypt(from, flen, &to[0], padding);
    if (len >= 0) {
        to.resize(static_cast<size_t>(len));
    } else {
        to.clear();
    }
    return len;
}

int32_t RSACipher::publicEncrypt(const void* from, int flen, std::string& to, int padding) {
    int rsa_size = getPubRSASize();
    if (rsa_size <= 0) {
        to.clear();
        return -1;
    }
    to.resize(static_cast<size_t>(rsa_size));
    int32_t len = publicEncrypt(from, flen, &to[0], padding);
    if (len >= 0) {
        to.resize(static_cast<size_t>(len));
    } else {
        to.clear();
    }
    return len;
}

int32_t RSACipher::privateDecrypt(const void* from, int flen, std::string& to, int padding) {
    int rsa_size = getPriRSASize();
    if (rsa_size <= 0) {
        to.clear();
        return -1;
    }
    to.resize(static_cast<size_t>(rsa_size));
    int32_t len = privateDecrypt(from, flen, &to[0], padding);
    if (len >= 0) {
        to.resize(static_cast<size_t>(len));
    } else {
        to.clear();
    }
    return len;
}

int32_t RSACipher::publicDecrypt(const void* from, int flen, std::string& to, int padding) {
    int rsa_size = getPubRSASize();
    if (rsa_size <= 0) {
        to.clear();
        return -1;
    }
    to.resize(static_cast<size_t>(rsa_size));
    int32_t len = publicDecrypt(from, flen, &to[0], padding);
    if (len >= 0) {
        to.resize(static_cast<size_t>(len));
    } else {
        to.clear();
    }
    return len;
}

int32_t RSACipher::getPubRSASize() {
    if (m_pubkey) {
        return RSA_size(m_pubkey);
    }
    return -1;
}

int32_t RSACipher::getPriRSASize() {
    if (m_prikey) {
        return RSA_size(m_prikey);
    }
    return -1;
}

}  // namespace CIM

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif