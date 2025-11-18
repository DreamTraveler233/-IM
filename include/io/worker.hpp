/**
 * @file    worker.hpp
 * @brief   协程/线程调度与工作线程池管理相关接口定义
 * @author  DreamTraveler233
 * @date    2023-01-01
 * @note    提供WorkerGroup与WorkerManager等核心调度组件。
 */

#ifndef __CIM_WORKER_HPP__
#define __CIM_WORKER_HPP__

#include "base/macro.hpp"
#include "base/singleton.hpp"
#include "io/iomanager.hpp"
#include "io/lock.hpp"

namespace CIM {

/**
     * @class   WorkerGroup
     * @brief   批量任务调度与同步的工作组
     *
     * 支持将一批任务分配到指定Scheduler上并同步等待全部完成。
     * 适用于批量异步任务的并发调度与统一收敛。
     * @note    WorkerGroup不可拷贝，仅支持智能指针管理。
     */
class WorkerGroup : Noncopyable, public std::enable_shared_from_this<WorkerGroup> {
   public:
    typedef std::shared_ptr<WorkerGroup> ptr;  ///< WorkerGroup智能指针类型

    /**
         * @brief   创建WorkerGroup实例
         * @param   batch_size  批量任务数
         * @param   s          调度器指针，默认取当前线程Scheduler
         * @return  WorkerGroup智能指针
         * @note    推荐通过Create静态方法创建
         */
    static WorkerGroup::ptr Create(uint32_t batch_size, Scheduler* s = Scheduler::GetThis()) {
        return std::make_shared<WorkerGroup>(batch_size, s);
    }

    /**
         * @brief   构造函数
         * @param   batch_size  批量任务数
         * @param   s          调度器指针
         */
    WorkerGroup(uint32_t batch_size, Scheduler* s = Scheduler::GetThis());

    /**
         * @brief   析构函数
         */
    ~WorkerGroup();

    /**
         * @brief   调度单个任务到工作组
         * @param   cb      待调度的回调函数
         * @param   thread  指定线程号，-1为任意线程
         */
    void schedule(std::function<void()> cb, int thread = -1);

    /**
         * @brief   等待所有任务完成
         * @note    阻塞当前线程直到所有任务执行完毕
         */
    void waitAll();

   private:
    /**
         * @brief   实际执行任务的内部方法
         * @param   cb  任务回调
         */
    void doWork(std::function<void()> cb);

   private:
    uint32_t m_batchSize;      ///< 批量任务数
    bool m_finish;             ///< 是否全部完成
    Scheduler* m_scheduler;    ///< 调度器指针
    CoroutineSemaphore m_sem;  ///< 协程信号量，用于同步
};

/**
     * @class   WorkerManager
     * @brief   全局工作线程池与调度器管理器
     *
     * 支持多命名线程池/调度器的注册、查找、批量调度等。
     * 适用于大规模并发服务的线程/协程资源统一管理。
     * @note    推荐通过WorkerMgr单例访问。
     */
class WorkerManager {
   public:
    /**
         * @brief   构造函数
         */
    WorkerManager();

    /**
         * @brief   添加调度器
         * @param   s  调度器智能指针
         */
    void add(Scheduler::ptr s);

    /**
         * @brief   获取指定名称的调度器
         * @param   name  调度器名称
         * @return  调度器智能指针，未找到返回nullptr
         */
    Scheduler::ptr get(const std::string& name);

    /**
         * @brief   获取指定名称的IOManager
         * @param   name  IOManager名称
         * @return  IOManager智能指针，未找到返回nullptr
         */
    IOManager::ptr getAsIOManager(const std::string& name);

    /**
         * @brief   向指定调度器调度单个任务
         * @tparam  FiberOrCb  协程或回调类型
         * @param   name       调度器名称
         * @param   fc         协程或回调
         * @param   thread     指定线程号，-1为任意线程
         * @note    若调度器不存在会记录错误日志
         */
    template <class FiberOrCb>
    void schedule(const std::string& name, FiberOrCb fc, int thread = -1) {
        auto s = get(name);
        if (s) {
            s->schedule(fc, thread);
        } else {
            static Logger::ptr s_logger = CIM_LOG_NAME("system");
            CIM_LOG_ERROR(s_logger) << "schedule name=" << name << " not exists";
        }
    }

    /**
         * @brief   向指定调度器批量调度任务
         * @tparam  Iter  迭代器类型
         * @param   name  调度器名称
         * @param   begin 任务起始迭代器
         * @param   end   任务结束迭代器
         * @note    若调度器不存在会记录错误日志
         */
    template <class Iter>
    void schedule(const std::string& name, Iter begin, Iter end) {
        auto s = get(name);
        if (s) {
            s->schedule(begin, end);
        } else {
            static Logger::ptr s_logger = CIM_LOG_NAME("system");
            CIM_LOG_ERROR(s_logger) << "schedule name=" << name << " not exists";
        }
    }

    /**
         * @brief   初始化所有调度器
         * @return  是否初始化成功
         */
    bool init();

    /**
         * @brief   按配置初始化调度器
         * @param   v  配置映射表
         * @return  是否初始化成功
         */
    bool init(const std::map<std::string, std::map<std::string, std::string>>& v);

    /**
         * @brief   停止所有调度器
         */
    void stop();

    /**
         * @brief   判断是否已全部停止
         * @return  true为已停止，false为运行中
         */
    bool isStoped() const { return m_stop; }

    /**
         * @brief   打印所有调度器信息
         * @param   os  输出流
         * @return  输出流引用
         */
    std::ostream& dump(std::ostream& os);

    /**
         * @brief   获取调度器总数
         * @return  调度器数量
         */
    uint32_t getCount();

   private:
    std::map<std::string, std::vector<Scheduler::ptr>> m_datas;  ///< 名称到调度器列表的映射
    bool m_stop;                                                 ///< 是否已全部停止
};

/**
     * @typedef WorkerMgr
     * @brief   WorkerManager单例类型定义
     * @note    推荐通过WorkerMgr::GetInstance()访问
     */
using WorkerMgr = Singleton<WorkerManager>;

}  // namespace CIM

#endif