#include "system/env.hpp"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iomanip>
#include <iostream>

#include "config/config.hpp"
#include "base/macro.hpp"

namespace CIM {
static auto g_logger = CIM_LOG_NAME("system");

bool Env::init(int argc, char** argv) {
    /**
         * 获取可执行文件的绝对路径
         */
    char link[1024] = {0};
    char path[1024] = {0};
    sprintf(link, "/proc/%d/exe", getpid());
    if (readlink(link, path, sizeof(path)) == -1) {
        // 处理错误情况
        perror("readlink");
    }
    // /path/xxx/exe
    m_exe = path;

    /**
         * 确定当前工作目录
         */
    auto pos = m_exe.find_last_of("/");
    m_cwd = m_exe.substr(0, pos) + "/";

    /**
         * 处理程序名称和命令行参数
         */
    m_program = argv[0];
    // -config /path/to/config -file xxxx -d
    const char* now_key = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (strlen(argv[i]) > 1) {
                if (now_key) {
                    add(now_key, "");
                }
                now_key = argv[i] + 1;
            } else {
                CIM_LOG_ERROR(g_logger) << "invalid arg idx=" << i << " val=" << argv[i];
                return false;
            }
        } else {
            if (now_key) {
                add(now_key, argv[i]);
                now_key = nullptr;
            } else {
                CIM_LOG_ERROR(g_logger) << "invalid arg idx=" << i << " val=" << argv[i];
                return false;
            }
        }
    }
    if (now_key) {
        add(now_key, "");
    }
    return true;
}

void Env::add(const std::string& key, const std::string& val) {
    CIM_ASSERT(!key.empty());
    RWMutexType::WriteLock lock(m_mutex);
    m_args[key] = val;
}

bool Env::has(const std::string& key) {
    CIM_ASSERT(!key.empty());
    RWMutexType::ReadLock lock(m_mutex);
    auto it = m_args.find(key);
    return it != m_args.end();
}

void Env::del(const std::string& key) {
    CIM_ASSERT(!key.empty());
    RWMutexType::WriteLock lock(m_mutex);
    m_args.erase(key);
}

std::string Env::get(const std::string& key, const std::string& default_value) {
    CIM_ASSERT(!key.empty());
    RWMutexType::ReadLock lock(m_mutex);
    auto it = m_args.find(key);
    return it != m_args.end() ? it->second : default_value;
}

void Env::addHelp(const std::string& key, const std::string& desc) {
    CIM_ASSERT(!key.empty());
    removeHelp(key);
    RWMutexType::WriteLock lock(m_mutex);
    m_helps.push_back(std::make_pair(key, desc));
}

void Env::removeHelp(const std::string& key) {
    CIM_ASSERT(!key.empty());
    RWMutexType::WriteLock lock(m_mutex);
    for (auto it = m_helps.begin(); it != m_helps.end();) {
        if (it->first == key) {
            it = m_helps.erase(it);
        } else {
            ++it;
        }
    }
}

void Env::printHelp() {
    RWMutexType::ReadLock lock(m_mutex);
    std::cout << "Usage: " << m_program << " [options]" << std::endl;
    for (auto& i : m_helps) {
        std::cout << std::setw(5) << "-" << i.first << " : " << i.second << std::endl;
    }
}

bool Env::setEnv(const std::string& key, const std::string& val) {
    CIM_ASSERT(!key.empty() && !val.empty());
    return !setenv(key.c_str(), val.c_str(), 1);
}

std::string Env::getEnv(const std::string& key, const std::string& default_value) {
    CIM_ASSERT(!key.empty());
    const char* v = getenv(key.c_str());
    if (v == nullptr) {
        return default_value;
    }
    return v;
}

std::string Env::getAbsolutePath(const std::string& path) const {
    if (path.empty())  // 输入路径为空，返回根路径
    {
        return "/";
    }
    if (path[0] == '/')  // 当前已是绝对路径
    {
        return path;
    }
    return m_cwd + path;  // 当前工作路径 + 输入路径
}

std::string Env::getAbsoluteWorkPath(const std::string& path) const {
    if (path.empty()) {
        return "/";
    }
    if (path[0] == '/') {
        return path;
    }
    static auto g_server_work_path = CIM::Config::Lookup<std::string>("server.work_path");
    return g_server_work_path->getValue() + "/" + path;
}

std::string Env::getConfigPath() {
    return getAbsolutePath(get("c", "config"));
}

}  // namespace CIM