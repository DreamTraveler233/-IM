#ifndef __CIM_ROCK_SERVER_HPP__
#define __CIM_ROCK_SERVER_HPP__

#include "rock_stream.hpp"
#include "net/tcp_server.hpp"

namespace CIM {
class RockServer : public TcpServer {
   public:
    typedef std::shared_ptr<RockServer> ptr;
    RockServer(const std::string& type = "rock", IOManager* worker = IOManager::GetThis(),
               IOManager* io_worker = IOManager::GetThis(),
               IOManager* accept_worker = IOManager::GetThis());

   protected:
    virtual void handleClient(Socket::ptr client) override;
};

}  // namespace CIM

#endif