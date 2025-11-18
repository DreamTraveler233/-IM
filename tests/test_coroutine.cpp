#include "base/macro.hpp"
#include "coroutine.hpp"

static auto g_logger = CIM_LOG_ROOT();

void run_in_coroutine()
{
    CIM_LOG_INFO(g_logger) << "run in coroutine begin";
    CIM::Coroutine::GetThis()->YieldToHold(); // 从子协程返回主协程
    CIM_LOG_INFO(g_logger) << "run in coroutine end";
    CIM::Coroutine::GetThis()->YieldToHold();
}

void test_fiber()
{
    // 创建当前线程的主协程
    CIM::Coroutine::GetThis();
    CIM_LOG_INFO(g_logger) << "main begin -1";
    {
        CIM_LOG_INFO(g_logger) << "main begin";
        // 创建子协程
        CIM::Coroutine::ptr coroutine = std::make_shared<CIM::Coroutine>(run_in_coroutine);
        coroutine->swapIn(); // 从当前线程的协程（主协程）进入子协程
        CIM_LOG_INFO(g_logger) << "main after swapIn";
        coroutine->swapIn(); // 进入子协程
        CIM_LOG_INFO(g_logger) << "main end";
        coroutine->swapIn();
    }
    CIM_LOG_INFO(g_logger) << "main after 2";
}

int main(int argc, char **argv)
{
    CIM::Thread::SetName("main");
    CIM_LOG_INFO(g_logger) << "main";
    std::vector<CIM::Thread::ptr> thrs;
    for(int i = 0; i < 5; ++i)
    {
        thrs.push_back(std::make_shared<CIM::Thread>(&test_fiber,"thread_"+std::to_string(i)));
    }
    for(auto i : thrs)
    {
        i->join();
    }
    return 0;
}