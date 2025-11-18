#include "config/config.hpp"

#include <sys/stat.h>

#include "system/env.hpp"

namespace CIM {
static auto g_logger = CIM_LOG_NAME("system");

/**
     * @brief 递归遍历YAML节点，将所有配置项的名称和节点存入输出列表
     * @param prefix 配置项名称前缀
     * @param node 当前处理的YAML节点
     * @param output 用于存储配置项名称和对应节点的输出列表
     *
     * 该函数首先检查配置项名称是否合法，然后将当前节点加入输出列表。
     * 如果当前节点是Map类型，则递归处理其所有子节点。
     * 配置项名称只能包含字母、数字、下划线和点号。
     */
static void ListAllMember(const std::string& prefix, const YAML::Node& node,
                          std::list<std::pair<std::string, const YAML::Node>>& output) {
    // 检查配置项名称是否合法，只能包含字母、数字、下划线和点号
    if (prefix.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._") != std::string::npos) {
        CIM_LOG_ERROR(g_logger) << "Config invalid name " << prefix << " : " << node;
        return;
    }
    // 将当前节点添加到输出列表
    output.push_back(std::make_pair(prefix, node));
    // 如果当前节点是Map类型，则递归处理其子节点
    if (node.IsMap()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            // 根据是否有前缀，构建子节点的完整名称并递归处理
            ListAllMember(prefix.empty() ? it->first.Scalar() : prefix + "." + it->first.Scalar(),
                          it->second, output);
        }
    }
}

ConfigVariableBase::ptr Config::LookupBase(const std::string& name) {
    RWMutexType::ReadLock lock(GetMutex());
    auto it = GetDatas().find(name);
    return it == GetDatas().end() ? nullptr : it->second;
}

// 文件修改时间
static std::map<std::string, uint64_t> s_file2modifytime;
static CIM::Mutex s_mutex;

/**
     * @brief 从指定目录加载配置文件
     * @param path 配置文件目录路径
     * @param force 是否强制加载，即使文件未修改也重新加载
     *
     * 该函数会遍历指定目录下的所有.yml文件，检查文件的修改时间，
     * 如果文件有更新或者强制加载标志为true，则加载该配置文件。
     * 加载成功后会通过LoadFromYaml函数将配置项注册到系统中。
     */
void Config::LoadFromConfigDir(const std::string& path, bool force) {
    CIM_ASSERT(!path.empty());
    // 获取绝对路径
    std::string absoulte_path = EnvMgr::GetInstance()->getAbsolutePath(path);
    // 存储找到的配置文件列表
    std::vector<std::string> files;
    // 遍历目录下所有.yml文件
    FSUtil::ListAllFile(files, absoulte_path, ".yaml");
    // 遍历所有找到的配置文件
    for (auto& i : files) {
        // 检查文件修改时间，避免重复加载未修改的文件
        {
            struct stat st;
            // 获取文件状态信息
            lstat(i.c_str(), &st);
            Mutex::Lock lock(s_mutex);
            // 如果非强制加载且文件修改时间未变，则跳过该文件
            if (!force && s_file2modifytime[i] == (uint64_t)st.st_mtime) {
                continue;
            }
            // 更新文件修改时间记录
            s_file2modifytime[i] = st.st_mtime;
        }
        try {
            // 加载配置文件
            YAML::Node root = YAML::LoadFile(i);
            LoadFromYaml(root);
            CIM_LOG_INFO(g_logger) << "LoadConfigFile file=" << i << " ok";
        } catch (...) {
            CIM_LOG_ERROR(g_logger) << "LoadConfigFile file=" << i << " failed";
        }
    }
}

/**
     * @brief 从YAML节点加载配置项
     * @param root YAML根节点
     *
     * 该函数递归遍历YAML节点树，将所有配置项的名称和值存入配置管理器中。
     * 配置项名称会被转换为小写，并通过lookupBase查找已存在的配置项，
     * 然后使用fromString方法更新配置项的值。
     */
void Config::LoadFromYaml(const YAML::Node& root) {
    CIM_ASSERT(root);

    std::list<std::pair<std::string, const YAML::Node>> all_nodes;
    // 递归遍历YAML节点树，将所有配置项的名称和值存入列表
    ListAllMember("", root, all_nodes);

    for (auto& i : all_nodes) {
        std::string key = i.first;  // 配置项名称
        if (key.empty()) {
            continue;
        }

        // 将配置项名称转换为小写
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        // 查找已存在的配置项
        ConfigVariableBase::ptr var = LookupBase(key);

        if (var)  // 如果找到了对应的配置变量
        {
            if (i.second.IsScalar())  // 如果YAML节点是标量类型（基本数据类型）
            {
                var->fromString(i.second.Scalar());  // 直接获取标量值并转换
            } else                                   // 如果是复杂类型（如Map或Sequence）
            {
                std::stringstream ss;
                ss << i.second;             // 将整个节点输出到字符串流
                var->fromString(ss.str());  // 转换为字符串后赋值给配置变量
            }
        }
        // CIM_LOG_DEBUG(g_logger) << std::endl
        //                         << LoggerMgr::GetInstance()->toYamlString();
    }
}

void Config::Visit(std::function<void(ConfigVariableBase::ptr)> cb) {
    CIM_ASSERT(cb);
    RWMutexType::ReadLock lock(GetMutex());
    ConfigVarMap& m = GetDatas();
    for (auto it = m.begin(); it != m.end(); ++it) {
        cb(it->second);
    }
}
}  // namespace CIM