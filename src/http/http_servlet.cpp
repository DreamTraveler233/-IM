#include "http/http_servlet.hpp"

#include <fnmatch.h>

namespace CIM::http {
Servlet::Servlet(const std::string& name) : m_name(name) {}
Servlet::~Servlet() {}
FunctionServlet::FunctionServlet(callback cb) : Servlet("FunctionServlet"), m_cb(cb) {}

int32_t FunctionServlet::handle(HttpRequest::ptr request, HttpResponse::ptr response,
                                HttpSession::ptr session) {
    return m_cb(request, response, session);
}

ServletDispatch::ServletDispatch() : Servlet("ServletDispatch") {
    m_default.reset(new NotFoundServlet("CIM/1.0"));
}

int32_t ServletDispatch::handle(HttpRequest::ptr request, http::HttpResponse::ptr response,
                                HttpSession::ptr session) {
    auto slt = getMatchedServlet(request->getPath());
    if (slt) {
        slt->handle(request, response, session);
    }
    return 0;
}

void ServletDispatch::addServlet(const std::string& uri, Servlet::ptr slt) {
    RWMutexType::WriteLock lock(m_mutex);
    m_datas[uri] = std::make_shared<HoldServletCreator>(slt);
}

void ServletDispatch::addServletCreator(const std::string& uri, IServletCreator::ptr creator) {
    RWMutexType::WriteLock lock(m_mutex);
    m_datas[uri] = creator;
}

void ServletDispatch::addGlobServletCreator(const std::string& uri, IServletCreator::ptr creator) {
    RWMutexType::WriteLock lock(m_mutex);
    for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
        if (it->first == uri) {
            m_globs.erase(it);
            break;
        }
    }
    m_globs.push_back(std::make_pair(uri, creator));
}

void ServletDispatch::addServlet(const std::string& uri, FunctionServlet::callback cb) {
    RWMutexType::WriteLock lock(m_mutex);
    m_datas[uri] = std::make_shared<HoldServletCreator>(std::make_shared<FunctionServlet>(cb));
}

void ServletDispatch::addGlobServlet(const std::string& uri, Servlet::ptr slt) {
    RWMutexType::WriteLock lock(m_mutex);
    for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
        if (it->first == uri) {
            m_globs.erase(it);
            break;
        }
    }
    m_globs.push_back(std::make_pair(uri, std::make_shared<HoldServletCreator>(slt)));
}

void ServletDispatch::addGlobServlet(const std::string& uri, FunctionServlet::callback cb) {
    return addGlobServlet(uri, std::make_shared<FunctionServlet>(cb));
}

void ServletDispatch::delServlet(const std::string& uri) {
    RWMutexType::WriteLock lock(m_mutex);
    m_datas.erase(uri);
}

void ServletDispatch::delGlobServlet(const std::string& uri) {
    RWMutexType::WriteLock lock(m_mutex);
    for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
        if (it->first == uri) {
            m_globs.erase(it);
            break;
        }
    }
}

Servlet::ptr ServletDispatch::getServlet(const std::string& uri) {
    RWMutexType::ReadLock lock(m_mutex);
    auto it = m_datas.find(uri);
    return it == m_datas.end() ? nullptr : it->second->get();
}

Servlet::ptr ServletDispatch::getGlobServlet(const std::string& uri) {
    RWMutexType::ReadLock lock(m_mutex);
    for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
        if (it->first == uri) {
            return it->second->get();
        }
    }
    return nullptr;
}

Servlet::ptr ServletDispatch::getMatchedServlet(const std::string& uri) {
    RWMutexType::ReadLock lock(m_mutex);
    // 精准匹配
    auto mit = m_datas.find(uri);
    if (mit != m_datas.end()) {
        return mit->second->get();
    }

    // 模糊匹配
    for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
        if (!fnmatch(it->first.c_str(), uri.c_str(), 0)) {
            return it->second->get();
        }
    }

    // 返回默认值
    return m_default;
}

void ServletDispatch::listAllServletCreator(std::map<std::string, IServletCreator::ptr>& infos) {
    RWMutexType::ReadLock lock(m_mutex);
    for (auto& i : m_datas) {
        infos[i.first] = i.second;
    }
}

void ServletDispatch::listAllGlobServletCreator(
    std::map<std::string, IServletCreator::ptr>& infos) {
    RWMutexType::ReadLock lock(m_mutex);
    for (auto& i : m_globs) {
        infos[i.first] = i.second;
    }
}

NotFoundServlet::NotFoundServlet(const std::string& name)
    : Servlet("NotFoundServlet"), m_name(name) {
    m_content =
        "<html><head><title>404 Not Found"
        "</title></head><body><center><h1>404 Not Found</h1></center>"
        "<hr><center>" +
        name + "</center></body></html>";
}

int32_t NotFoundServlet::handle(HttpRequest::ptr request, HttpResponse::ptr response,
                                HttpSession::ptr session) {
    response->setStatus(HttpStatus::NOT_FOUND);
    response->setHeader("Server", "CIM/1.0.0");

    // 如果是 API 路径或客户端期望 JSON，则返回统一 JSON 错误，避免前端解析 HTML 报错
    const std::string& path = request->getPath();
    const std::string accept = request->getHeader("Accept", "");
    if ((path.size() >= 5 && path.rfind("/api/", 0) == 0) ||
        accept.find("application/json") != std::string::npos) {
        response->setHeader("Content-Type", "application/json; charset=utf-8");
        response->setBody("{\"code\":404,\"message\":\"not found\"}");
    } else {
        response->setHeader("Content-Type", "text/html");
        response->setBody(m_content);
    }
    return 0;
}
const std::string& Servlet::getName() const {
    return m_name;
}
Servlet::ptr ServletDispatch::getDefault() const {
    return m_default;
}
void ServletDispatch::setDefault(Servlet::ptr v) {
    m_default = v;
}
}  // namespace CIM::http