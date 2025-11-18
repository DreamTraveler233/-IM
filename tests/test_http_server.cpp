#include "http_server.hpp"
#include "base/macro.hpp"
#include "http_servlet.hpp"

void test()
{
    CIM::http::HttpServer::ptr http_server(new CIM::http::HttpServer);
    CIM::Address::ptr addr = CIM::Address::LookupAnyIpAddress("0.0.0.0:8020");
    while (!http_server->bind(addr))
    {
        sleep(2);
    }

    auto sb = http_server->getServletDispatch();
    sb->addServlet("/CIM/xx", [](CIM::http::HttpRequest::ptr req,
                                   CIM::http::HttpResponse::ptr res,
                                   CIM::http::HttpSession::ptr session)
                   { 
                    res->setBody(req->toString()); 
                    return 0; });

    sb->addGlobServlet("/CIM/*", [](CIM::http::HttpRequest::ptr req,
                                      CIM::http::HttpResponse::ptr res,
                                      CIM::http::HttpSession::ptr session)
                       { 
                        res->setBody("Glob: \r\n" + req->toString());
                        return 0; });

    http_server->start();
}

int main(int argc, char **argv)
{
    CIM::IOManager iom(2);
    iom.schedule(test);
    return 0;
}