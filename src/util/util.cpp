#include "util/util.hpp"

#include <arpa/inet.h>
#include <dirent.h>
#include <execinfo.h>
#include <google/protobuf/unknown_field_set.h>
#include <ifaddrs.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "io/coroutine.hpp"
#include "base/macro.hpp"

namespace CIM {
static auto g_logger = CIM_LOG_NAME("system");

pid_t GetThreadId() {
    return syscall(SYS_gettid);
}

uint64_t GetCoroutineId() {
    return Coroutine::GetCoroutineId();
}

void Backtrace(std::vector<std::string>& bt, int size, int skip) {
    // 分配存储堆栈地址的数组空间
    void** array = (void**)malloc(sizeof(void*) * size);
    // 获取函数调用堆栈地址
    size_t s = ::backtrace(array, size);

    // 将地址转换为可读的符号字符串
    char** strings = backtrace_symbols(array, s);
    if (NULL == strings) {
        CIM_LOG_ERROR(g_logger) << "backtrace_symbols error";
        free(array);
        return;
    }

    // 将堆栈符号信息存储到输出向量中
    for (size_t i = skip; i < s; ++i) {
        bt.push_back(strings[i]);
    }

    // 释放分配的内存
    free(array);
    free(strings);
}

std::string BacktraceToString(int size, int skip, const std::string& prefix) {
    std::vector<std::string> bt;
    Backtrace(bt, size, skip);
    std::stringstream ss;
    for (size_t i = 0; i < bt.size(); ++i) {
        ss << prefix << bt[i] << std::endl;
    }

    return ss.str();
}

void FSUtil::ListAllFile(std::vector<std::string>& files, const std::string& path,
                         const std::string& subfix) {
    if (access(path.c_str(), 0) != 0) {
        return;
    }
    DIR* dir = opendir(path.c_str());
    if (dir == nullptr) {
        return;
    }
    struct dirent* dp = nullptr;
    while ((dp = readdir(dir)) != nullptr) {
        if (dp->d_type == DT_DIR) {
            if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) {
                continue;
            }
            ListAllFile(files, path + "/" + dp->d_name, subfix);
        } else if (dp->d_type == DT_REG) {
            std::string filename(dp->d_name);
            if (subfix.empty()) {
                files.push_back(path + "/" + filename);
            } else {
                if (filename.size() < subfix.size()) {
                    continue;
                }
                if (filename.substr(filename.length() - subfix.size()) == subfix) {
                    files.push_back(path + "/" + filename);
                }
            }
        }
    }
    closedir(dir);
}

static int __lstat(const char* file, struct stat* st = nullptr) {
    struct stat lst;
    int ret = lstat(file, &lst);
    if (st) {
        *st = lst;
    }
    return ret;
}

static int __mkdir(const char* dirname) {
    if (access(dirname, F_OK) == 0) {
        return 0;
    }
    return mkdir(dirname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

bool FSUtil::Mkdir(const std::string& dirname) {
    if (__lstat(dirname.c_str()) == 0) {
        return true;
    }
    char* path = strdup(dirname.c_str());
    char* ptr = strchr(path + 1, '/');
    do {
        for (; ptr; *ptr = '/', ptr = strchr(ptr + 1, '/')) {
            *ptr = '\0';
            if (__mkdir(path) != 0) {
                break;
            }
        }
        if (ptr != nullptr) {
            break;
        } else if (__mkdir(path) != 0) {
            break;
        }
        free(path);
        return true;
    } while (0);
    free(path);
    return false;
}

bool FSUtil::IsRunningPidfile(const std::string& pidfile) {
    if (__lstat(pidfile.c_str()) != 0) {
        return false;
    }
    std::ifstream ifs(pidfile);
    std::string line;
    if (!ifs || !std::getline(ifs, line)) {
        return false;
    }
    if (line.empty()) {
        return false;
    }
    pid_t pid = atoi(line.c_str());
    if (pid <= 1) {
        return false;
    }
    if (kill(pid, 0) != 0) {
        return false;
    }
    return true;
}

bool FSUtil::Unlink(const std::string& filename, bool exist) {
    if (!exist && __lstat(filename.c_str())) {
        return true;
    }
    return ::unlink(filename.c_str()) == 0;
}

bool FSUtil::Rm(const std::string& path) {
    struct stat st;
    if (lstat(path.c_str(), &st)) {
        return true;
    }
    if (!(st.st_mode & S_IFDIR)) {
        return Unlink(path);
    }

    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return false;
    }

    bool ret = true;
    struct dirent* dp = nullptr;
    while ((dp = readdir(dir))) {
        if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) {
            continue;
        }
        std::string dirname = path + "/" + dp->d_name;
        ret = Rm(dirname);
    }
    closedir(dir);
    if (::rmdir(path.c_str())) {
        ret = false;
    }
    return ret;
}

bool FSUtil::Mv(const std::string& from, const std::string& to) {
    if (!Rm(to)) {
        return false;
    }
    return rename(from.c_str(), to.c_str()) == 0;
}

bool FSUtil::Realpath(const std::string& path, std::string& rpath) {
    if (__lstat(path.c_str())) {
        return false;
    }
    char* ptr = ::realpath(path.c_str(), nullptr);
    if (nullptr == ptr) {
        return false;
    }
    std::string(ptr).swap(rpath);
    free(ptr);
    return true;
}

bool FSUtil::Symlink(const std::string& from, const std::string& to) {
    if (!Rm(to)) {
        return false;
    }
    return ::symlink(from.c_str(), to.c_str()) == 0;
}

std::string FSUtil::Dirname(const std::string& filename) {
    if (filename.empty()) {
        return ".";
    }
    auto pos = filename.rfind('/');
    if (pos == 0) {
        return "/";
    } else if (pos == std::string::npos) {
        return ".";
    } else {
        return filename.substr(0, pos);
    }
}

std::string FSUtil::Basename(const std::string& filename) {
    if (filename.empty()) {
        return filename;
    }
    auto pos = filename.rfind('/');
    if (pos == std::string::npos) {
        return filename;
    } else {
        return filename.substr(pos + 1);
    }
}

bool FSUtil::OpenForRead(std::ifstream& ifs, const std::string& filename,
                         std::ios_base::openmode mode) {
    ifs.open(filename.c_str(), mode);
    return ifs.is_open();
}

bool FSUtil::OpenForWrite(std::ofstream& ofs, const std::string& filename,
                          std::ios_base::openmode mode) {
    ofs.open(filename.c_str(), mode);
    if (!ofs.is_open()) {
        std::string dir = Dirname(filename);
        Mkdir(dir);
        ofs.open(filename.c_str(), mode);
    }
    return ofs.is_open();
}

bool YamlToJson(const YAML::Node& ynode, Json::Value& jnode) {
    try {
        if (ynode.IsScalar()) {
            Json::Value v(ynode.Scalar());
            jnode.swapPayload(v);
            return true;
        }
        if (ynode.IsSequence()) {
            for (size_t i = 0; i < ynode.size(); ++i) {
                Json::Value v;
                if (YamlToJson(ynode[i], v)) {
                    jnode.append(v);
                } else {
                    return false;
                }
            }
        } else if (ynode.IsMap()) {
            for (auto it = ynode.begin(); it != ynode.end(); ++it) {
                Json::Value v;
                if (YamlToJson(it->second, v)) {
                    jnode[it->first.Scalar()] = v;
                } else {
                    return false;
                }
            }
        }
    } catch (...) {
        return false;
    }
    return true;
}

bool JsonToYaml(const Json::Value& jnode, YAML::Node& ynode) {
    try {
        if (jnode.isArray()) {
            for (int i = 0; i < (int)jnode.size(); ++i) {
                YAML::Node n;
                if (JsonToYaml(jnode[i], n)) {
                    ynode.push_back(n);
                } else {
                    return false;
                }
            }
        } else if (jnode.isObject()) {
            for (auto it = jnode.begin(); it != jnode.end(); ++it) {
                YAML::Node n;
                if (JsonToYaml(*it, n)) {
                    ynode[it.name()] = n;
                } else {
                    return false;
                }
            }
        } else {
            ynode = jnode.asString();
        }
    } catch (...) {
        return false;
    }
    return true;
}

std::string GetHostName() {
    std::shared_ptr<char> host(new char[512], delete_array<char>);
    memset(host.get(), 0, 512);
    gethostname(host.get(), 511);
    return host.get();
}

in_addr_t GetIPv4Inet() {
    struct ifaddrs* ifas = nullptr;
    struct ifaddrs* ifa = nullptr;

    in_addr_t localhost = inet_addr("127.0.0.1");
    if (getifaddrs(&ifas)) {
        CIM_LOG_ERROR(g_logger) << "getifaddrs errno=" << errno << " errstr=" << strerror(errno);
        return localhost;
    }

    in_addr_t ipv4 = localhost;

    for (ifa = ifas; ifa && ifa->ifa_addr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        if (!strncasecmp(ifa->ifa_name, "lo", 2)) {
            continue;
        }
        ipv4 = ((struct sockaddr_in*)ifa->ifa_addr)->sin_addr.s_addr;
        if (ipv4 == localhost) {
            continue;
        }
    }
    if (ifas != nullptr) {
        freeifaddrs(ifas);
    }
    return ipv4;
}

std::string _GetIPv4() {
    std::shared_ptr<char> ipv4(new char[INET_ADDRSTRLEN], delete_array<char>);
    memset(ipv4.get(), 0, INET_ADDRSTRLEN);
    auto ia = GetIPv4Inet();
    inet_ntop(AF_INET, &ia, ipv4.get(), INET_ADDRSTRLEN);
    return ipv4.get();
}

std::string GetIPv4() {
    static const std::string ip = _GetIPv4();
    return ip;
}

std::string ToUpper(const std::string& name) {
    std::string rt = name;
    std::transform(rt.begin(), rt.end(), rt.begin(), ::toupper);
    return rt;
}

std::string ToLower(const std::string& name) {
    std::string rt = name;
    std::transform(rt.begin(), rt.end(), rt.begin(), ::tolower);
    return rt;
}

int8_t TypeUtil::ToChar(const std::string& str) {
    if (str.empty()) {
        return 0;
    }
    return *str.begin();
}

int64_t TypeUtil::Atoi(const std::string& str) {
    if (str.empty()) {
        return 0;
    }
    return strtoull(str.c_str(), nullptr, 10);
}

double TypeUtil::Atof(const std::string& str) {
    if (str.empty()) {
        return 0;
    }
    return atof(str.c_str());
}

int8_t TypeUtil::ToChar(const char* str) {
    if (str == nullptr) {
        return 0;
    }
    return str[0];
}

int64_t TypeUtil::Atoi(const char* str) {
    if (str == nullptr) {
        return 0;
    }
    return strtoull(str, nullptr, 10);
}

double TypeUtil::Atof(const char* str) {
    if (str == nullptr) {
        return 0;
    }
    return atof(str);
}
}  // namespace CIM