#include "http.hpp"
#include "base/macro.hpp"
#include <iostream>

static auto g_logger = CIM_LOG_ROOT();

void test_request()
{
    CIM::http::HttpRequest::ptr req(new CIM::http::HttpRequest);
    req->setHeader("host", "www.baidu.com");
    std::cout << req->toString() << std::endl;
}

void test_response()
{
    CIM::http::HttpResponse::ptr req(new CIM::http::HttpResponse);
    req->setHeader("X-X", "CIM");
    req->setBody("hello world");
    req->setStatus((CIM::http::HttpStatus)404);
    req->setClose(false);
    std::cout << req->toString() << std::endl;
}

int main(int argc, char **crgv)
{
    test_request();
    test_response();
    return 0;
}