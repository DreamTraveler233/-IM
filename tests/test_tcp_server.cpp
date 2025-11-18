#include "tcp_server.hpp"
#include "iomanager.hpp"
#include "base/macro.hpp"

CIM::Logger::ptr g_logger = CIM_LOG_ROOT();

void run()
{
    auto addr = CIM::Address::LookupAny("0.0.0.0:8033");
    CIM_LOG_INFO(g_logger) << *addr;
    //auto addr2 = CIM::UnixAddress::ptr(new CIM::UnixAddress("/tmp/unix_addr"));
    std::vector<CIM::Address::ptr> addrs;
    addrs.push_back(addr);
    //addrs.push_back(addr2);

    CIM::TcpServer::ptr tcp_server(new CIM::TcpServer);
    std::vector<CIM::Address::ptr> fails;
    while (!tcp_server->bind(addrs, fails))
    {
        sleep(2);
    }
    tcp_server->start();
}
int main(int argc, char **argv)
{
    CIM::IOManager iom(2);
    iom.schedule(run);
    return 0;
}