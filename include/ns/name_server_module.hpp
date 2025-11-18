#ifndef __CIM_NS_NAME_SERVER_MODULE_HPP__
#define __CIM_NS_NAME_SERVER_MODULE_HPP__

#include "other/module.hpp"
#include "ns_protocol.hpp"

namespace CIM::ns {

class NameServerModule;
class NSClientInfo {
    friend class NameServerModule;

   public:
    typedef std::shared_ptr<NSClientInfo> ptr;

   private:
    NSNode::ptr m_node;
    std::map<std::string, std::set<uint32_t>> m_domain2cmds;
};

class NameServerModule : public RockModule {
   public:
    typedef std::shared_ptr<NameServerModule> ptr;
    NameServerModule();

    virtual bool handleRockRequest(RockRequest::ptr request, RockResponse::ptr response,
                                   RockStream::ptr stream) override;
    virtual bool handleRockNotify(RockNotify::ptr notify, RockStream::ptr stream) override;
    virtual bool onConnect(Stream::ptr stream) override;
    virtual bool onDisconnect(Stream::ptr stream) override;
    virtual std::string statusString() override;

   private:
    bool handleRegister(RockRequest::ptr request, RockResponse::ptr response,
                        RockStream::ptr stream);
    bool handleQuery(RockRequest::ptr request, RockResponse::ptr response, RockStream::ptr stream);
    bool handleTick(RockRequest::ptr request, RockResponse::ptr response, RockStream::ptr stream);

   private:
    NSClientInfo::ptr get(RockStream::ptr rs);
    void set(RockStream::ptr rs, NSClientInfo::ptr info);

    void setQueryDomain(RockStream::ptr rs, const std::set<std::string>& ds);

    void doNotify(std::set<std::string>& domains, std::shared_ptr<NotifyMessage> nty);

    std::set<RockStream::ptr> getStreams(const std::string& domain);

   private:
    NSDomainSet::ptr m_domains;

    RWMutex m_mutex;
    std::map<RockStream::ptr, NSClientInfo::ptr> m_sessions;

    /// sessoin 关注的域名
    std::map<RockStream::ptr, std::set<std::string>> m_queryDomains;
    /// 域名对应关注的session
    std::map<std::string, std::set<RockStream::ptr>> m_domainToSessions;
};

}  // namespace CIM::ns

#endif