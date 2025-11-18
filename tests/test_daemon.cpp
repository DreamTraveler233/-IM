#include "daemon.hpp"
#include "iomanager.hpp"
#include "base/macro.hpp"

static CIM::Logger::ptr g_logger = CIM_LOG_ROOT();

CIM::Timer::ptr timer;
int server_main(int argc, char** argv) {
    CIM_LOG_INFO(g_logger) << CIM::ProcessInfoMgr::GetInstance()->toString();
    CIM::IOManager iom(1);
    timer = iom.addTimer(1000, [](){
            CIM_LOG_INFO(g_logger) << "onTimer";
            // static int count = 0;
            // if(++count > 10) {
            //     exit(1);
            // }
    }, true);
    return 0;
}

int main(int argc, char** argv) {
    return CIM::start_daemon(argc, argv, server_main, argc != 1);
}