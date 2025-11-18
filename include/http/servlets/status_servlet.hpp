#ifndef __CIM_HTTP_SERVLETS_STATUS_SERVLET_HPP__
#define __CIM_HTTP_SERVLETS_STATUS_SERVLET_HPP__

#include "http/http_servlet.hpp"

namespace CIM::http {

class StatusServlet : public Servlet {
   public:
    StatusServlet();
    virtual int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response,
                           HttpSession::ptr session) override;
};

}  // namespace CIM::http

#endif