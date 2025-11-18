#include "other/crypto_module.hpp"

#include "base/macro.hpp"
#include "system/application.hpp"
#include "util/util.hpp"

namespace CIM {
static auto g_logger = CIM_LOG_NAME("system");

// 配置项：路径与填充
static auto g_rsa_priv_path = CIM::Config::Lookup<std::string>(
    "crypto.rsa_private_key_path", std::string(""), "rsa private key path");
static auto g_rsa_pub_path = CIM::Config::Lookup<std::string>(
    "crypto.rsa_public_key_path", std::string(""), "rsa public key path (PKCS#1)");
static auto g_rsa_padding = CIM::Config::Lookup<std::string>("crypto.padding", std::string("OAEP"),
                                                             "rsa padding: OAEP|PKCS1|NOPAD");

CryptoModule::CryptoModule() : Module("crypto", "0.1.0", "builtin") {}

int CryptoModule::ParsePadding(const std::string& name) {
    std::string up;
    up.resize(name.size());
    std::transform(name.begin(), name.end(), up.begin(),
                   [](unsigned char c) { return static_cast<char>(::toupper(c)); });
    if (up == "OAEP") return RSA_PKCS1_OAEP_PADDING;
    if (up == "PKCS1" || up == "PKCS1_V15") return RSA_PKCS1_PADDING;
    if (up == "NOPAD" || up == "NO_PADDING") return RSA_NO_PADDING;
    // 默认 OAEP
    return RSA_PKCS1_OAEP_PADDING;
}

std::string CryptoModule::MakeAbsPath(const std::string& p) {
    if (p.empty()) return p;
    // 绝对路径直接返回
    if (!p.empty() && p[0] == '/') return p;

    // 依次尝试：以配置目录为基准、以可执行目录为基准、以工作目录为基准
    std::vector<std::string> candidates;
    auto env = EnvMgr::GetInstance();
    // config dir
    candidates.push_back(env->getConfigPath() + "/" + p);
    // exe dir
    candidates.push_back(env->getAbsolutePath(p));
    // work dir (server.work_path)
    candidates.push_back(env->getAbsoluteWorkPath(p));

    for (auto& cand : candidates) {
        std::ifstream ifs;
        if (CIM::FSUtil::OpenForRead(ifs, cand, std::ios::in)) {
            ifs.close();
            return cand;
        }
    }
    // 均未命中，回退到以可执行目录为基准的绝对路径（用于日志展示）
    return env->getAbsolutePath(p);
}

bool CryptoModule::onLoad() {
    const std::string priv_cfg = g_rsa_priv_path->getValue();
    const std::string pub_cfg = g_rsa_pub_path->getValue();
    const std::string pad_cfg = g_rsa_padding->getValue();

    auto priv_path = MakeAbsPath(priv_cfg);
    auto pub_path = MakeAbsPath(pub_cfg);
    m_padding = ParsePadding(pad_cfg);

    if (priv_path.empty() || pub_path.empty()) {
        CIM_LOG_ERROR(g_logger) << "CryptoModule: rsa key path not configured: private='"
                                << priv_cfg << "' public='" << pub_cfg << "'";
        return false;
    }

    // 尝试加载密钥
    m_rsa = CIM::RSACipher::Create(pub_path, priv_path);
    if (!m_rsa) {
        // 友好的提示：支持 PKCS#1 / PKCS#8 / SubjectPublicKeyInfo 多格式
        CIM_LOG_ERROR(g_logger) << "CryptoModule: load RSA keys failed. pub='" << pub_path
                                << "' pri='" << priv_path
                                << "'. Checked config/exe/work base paths and multiple PEM "
                                   "formats. Please verify file exists and readable.";
        return false;
    }

    // 基本可用性检查
    int pub_sz = m_rsa->getPubRSASize();
    int pri_sz = m_rsa->getPriRSASize();
    if (pub_sz <= 0 || pri_sz <= 0) {
        CIM_LOG_ERROR(g_logger) << "CryptoModule: RSA size invalid: pub=" << pub_sz
                                << " pri=" << pri_sz;
        m_rsa.reset();
        return false;
    }

    CIM_LOG_INFO(g_logger) << "CryptoModule: RSA loaded. pub_size=" << pub_sz
                           << " pri_size=" << pri_sz << " padding=" << pad_cfg;
    return true;
}

int CryptoModule::maxPlaintextLen() const {
    RWMutex::ReadLock lock(m_mutex);
    if (!m_rsa) return -1;
    int k = m_rsa->getPubRSASize();
    if (k <= 0) return -1;
    switch (m_padding) {
        case RSA_PKCS1_OAEP_PADDING:
            return k - 42;  // 采用 SHA-1 的典型上限
        case RSA_PKCS1_PADDING:
            return k - 11;
        case RSA_NO_PADDING:
            return k;
        default:
            return k - 11;  // 保守值
    }
}

bool CryptoModule::PublicEncrypt(const std::string& plaintext, std::string& ciphertext) const {
    RWMutex::ReadLock lock(m_mutex);
    if (!m_rsa) return false;
    int k = m_rsa->getPubRSASize();
    if (k <= 0) return false;
    // 明文长度校验（NOPAD 需要等长；其他需不超过上限）
    if (m_padding == RSA_NO_PADDING) {
        if ((int)plaintext.size() != k) return false;
    } else {
        int maxlen = (m_padding == RSA_PKCS1_OAEP_PADDING) ? (k - 42)
                     : (m_padding == RSA_PKCS1_PADDING)    ? (k - 11)
                                                           : (k - 11);
        if ((int)plaintext.size() > maxlen) return false;
    }
    int32_t ret =
        m_rsa->publicEncrypt(plaintext.data(), (int)plaintext.size(), ciphertext, m_padding);
    return ret >= 0;
}

bool CryptoModule::PrivateDecrypt(const std::string& ciphertext, std::string& plaintext) const {
    RWMutex::ReadLock lock(m_mutex);
    if (!m_rsa) return false;
    int k = m_rsa->getPriRSASize();
    if (k <= 0) return false;
    if ((int)ciphertext.size() != k) return false;
    int32_t ret =
        m_rsa->privateDecrypt(ciphertext.data(), (int)ciphertext.size(), plaintext, m_padding);
    return ret >= 0;
}

bool CryptoModule::PrivateEncrypt(const std::string& input, std::string& output) const {
    RWMutex::ReadLock lock(m_mutex);
    if (!m_rsa) return false;
    int k = m_rsa->getPriRSASize();
    if (k <= 0) return false;
    if (m_padding == RSA_NO_PADDING) {
        if ((int)input.size() != k) return false;
    } else {
        int maxlen = (m_padding == RSA_PKCS1_OAEP_PADDING) ? (k - 42)
                     : (m_padding == RSA_PKCS1_PADDING)    ? (k - 11)
                                                           : (k - 11);
        if ((int)input.size() > maxlen) return false;
    }
    int32_t ret = m_rsa->privateEncrypt(input.data(), (int)input.size(), output, m_padding);
    return ret >= 0;
}

bool CryptoModule::PublicDecrypt(const std::string& input, std::string& output) const {
    RWMutex::ReadLock lock(m_mutex);
    if (!m_rsa) return false;
    int k = m_rsa->getPubRSASize();
    if (k <= 0) return false;
    if ((int)input.size() != k) return false;
    int32_t ret = m_rsa->publicDecrypt(input.data(), (int)input.size(), output, m_padding);
    return ret >= 0;
}

CryptoModule::ptr CryptoModule::Get() {
    auto m = ModuleMgr::GetInstance()->get("crypto/0.1.0");
    return std::dynamic_pointer_cast<CryptoModule>(m);
}

}  // namespace CIM
