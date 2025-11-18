#pragma once

#include <unistd.h>

#include <functional>

#include "base/singleton.hpp"

namespace CIM {
/*
    工作原理总结
    这个守护进程实现采用双进程模型：

    父进程（监控进程）：负责创建和监控子进程，当子进程异常退出时能自动重启
    子进程（工作进程）：实际执行用户定义的主程序逻辑
     */
struct ProcessInfo {
    pid_t parent_id = 0;             /// 父进程id
    pid_t main_id = 0;               /// 主进程id
    uint64_t parent_start_time = 0;  /// 父进程启动时间
    uint64_t main_start_time = 0;    /// 主进程启动时间
    uint32_t restart_count = 0;      /// 主进程重启的次数
    std::string toString() const;
};

using ProcessInfoMgr = Singleton<ProcessInfo>;

/**
     * @brief 启动程序可以选择用守护进程的方式
     * @param[in] argc 参数个数
     * @param[in] argv 参数值数组
     * @param[in] main_cb 启动函数
     * @param[in] is_daemon 是否守护进程的方式
     * @return 返回程序的执行结果
     */
int start_daemon(int argc, char** argv, std::function<int(int argc, char** argv)> main_cb,
                 bool is_daemon);

}  // namespace CIM