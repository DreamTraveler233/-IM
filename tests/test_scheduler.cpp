#include "base/macro.hpp"
#include "scheduler.hpp"
#include"iomanager.hpp"

auto g_logger = CIM_LOG_ROOT();

void test_fiber()
{
    static int count = 5;
    CIM_LOG_INFO(g_logger) << "test begin count=" << count;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (--count>0)
    {
        CIM::IOManager::GetThis()->schedule(test_fiber);
    }
}

int main(int argc, char **argv)
{
    // CIM::Scheduler sc(1,true,"test");
    // sc.start();
    // sc.schedule(test_fiber);
    // sc.stop();

    CIM::IOManager iom(2,true,"test");
    iom.schedule(test_fiber);
    return 0;
}