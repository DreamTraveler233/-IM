#include "system/daemon.hpp"

#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "config/config.hpp"
#include "base/macro.hpp"
#include "util/util.hpp"

namespace CIM {
static auto g_logger = CIM_LOG_NAME("system");
static auto g_daemon_restart_interval =
    CIM::Config::Lookup("daemon.restart_interval", (uint32_t)5, "daemon restart interval");

std::string ProcessInfo::toString() const {
    std::stringstream ss;
    ss << "[ProcessInfo parent_id=" << parent_id << " main_id=" << main_id
       << " parent_start_time=" << TimeUtil::TimeToStr(parent_start_time)
       << " main_start_time=" << TimeUtil::TimeToStr(main_start_time)
       << " restart_count=" << restart_count << "]";
    return ss.str();
}

/**
     * @brief 实际启动应用程序的函数
     * @param[in] argc 参数个数
     * @param[in] argv 参数值数组
     * @param[in] main_cb 用户定义的主函数回调
     * @return 返回主函数回调的执行结果
     */
static int real_start(int argc, char** argv, std::function<int(int argc, char** argv)> main_cb) {
    // 记录子进程ID和启动时间
    ProcessInfoMgr::GetInstance()->main_id = getpid();
    ProcessInfoMgr::GetInstance()->main_start_time = time(0);
    return main_cb(argc, argv);
}

/**
     * @brief 守护进程主函数，负责创建和管理子进程
     * @param argc 命令行参数个数
     * @param argv 命令行参数数组
     * @param main_cb 主回调函数，当创建子进程时会调用该函数
     * @return int 执行结果，0表示正常退出，-1表示执行出错
     */
static int real_daemon(int argc, char** argv, std::function<int(int argc, char** argv)> main_cb) {
    // 将当前进程转换为守护进程，保持当前工作目录不变，并关闭文件描述符
    if (daemon(1, 0) == -1) {
        // 处理错误情况
        perror("daemon");
        return -1;
    }
    // 记录父进程ID和启动时间
    ProcessInfoMgr::GetInstance()->parent_id = getpid();
    ProcessInfoMgr::GetInstance()->parent_start_time = time(0);
    // 循环创建和监控子进程
    while (true) {
        // 创建子进程
        pid_t pid = fork();
        if (pid == 0) {
            // 子进程
            // 记录子进程ID和启动时间
            ProcessInfoMgr::GetInstance()->main_id = getpid();
            ProcessInfoMgr::GetInstance()->main_start_time = time(0);
            CIM_LOG_INFO(g_logger) << "process start pid=" << getpid();

            // 执行实际应用程序
            return real_start(argc, argv, main_cb);
        } else if (pid < 0) {
            // fork失败处理
            CIM_LOG_ERROR(g_logger) << "fork fail return=" << pid << " errno=" << errno
                                    << " errstr=" << strerror(errno);
            return -1;
        } else {
            // 父进程
            int status = 0;
            // 父进程阻塞，等待子进程返回
            waitpid(pid, &status, 0);
            if (status) {
                // 子进程异常退出
                if (WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL) {
                    // 子进程被kill，跳出循环，不再重启
                    CIM_LOG_INFO(g_logger) << "killed";
                    break;
                } else {
                    // 子进程崩溃，如段错误、其他信号等
                    CIM_LOG_ERROR(g_logger) << "child crash pid=" << pid << " status=" << status;
                }
            } else {
                // 子进程正常退出，跳出循环，不再重启
                CIM_LOG_INFO(g_logger) << "child finished pid=" << pid;
                break;
            }
            // 增加重启计数并等待指定时间后重新启动（用于等待上一次进程的资源释放）
            ProcessInfoMgr::GetInstance()->restart_count += 1;
            sleep(g_daemon_restart_interval->getValue());
        }
    }
    return 0;
}

int start_daemon(int argc, char** argv, std::function<int(int argc, char** argv)> main_cb,
                 bool is_daemon) {
    if (!is_daemon)  // 不启用守护进程
    {
        // 记录父进程ID和启动时间
        ProcessInfoMgr::GetInstance()->parent_id = getpid();
        ProcessInfoMgr::GetInstance()->parent_start_time = time(0);
        return real_start(argc, argv, main_cb);
    }
    return real_daemon(argc, argv, main_cb);
}

}  // namespace CIM