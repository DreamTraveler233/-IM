#ifndef __CIM_NS_NS_CLIENT_HPP__
#define __CIM_NS_NS_CLIENT_HPP__

#include "ns/ns_protocol.hpp"
#include "rock/rock_stream.hpp"

namespace CIM::ns {
class NSClient : public RockConnection {
   public:
    typedef std::shared_ptr<NSClient> ptr;
    NSClient();
    ~NSClient();

    const std::set<std::string>& getQueryDomains();
    void setQueryDomains(const std::set<std::string>& v);

    void addQueryDomain(const std::string& domain);
    void delQueryDomain(const std::string& domain);

    bool hasQueryDomain(const std::string& domain);

    RockResult::ptr query();

    void init();
    void uninit();
    NSDomainSet::ptr getDomains() const { return m_domains; }

   private:
    void onQueryDomainChange();
    bool onConnect(AsyncSocketStream::ptr stream);
    void onDisconnect(AsyncSocketStream::ptr stream);
    bool onNotify(RockNotify::ptr, RockStream::ptr);

    void onTimer();

   private:
    RWMutex m_mutex;
    std::set<std::string> m_queryDomains;
    NSDomainSet::ptr m_domains;
    uint32_t m_sn = 0;
    Timer::ptr m_timer;
};

}  // namespace CIM::ns

#endif