#include <iostream>
#include "http_connection.hpp"
#include "base/macro.hpp"
#include "iomanager.hpp"
#include "http_parser.hpp"
#include "zlib_stream.hpp"
#include <fstream>

static CIM::Logger::ptr g_logger = CIM_LOG_ROOT();

void test_pool()
{
    CIM::http::HttpConnectionPool::ptr pool(new CIM::http::HttpConnectionPool(
        "www.baidu.com", "", 80, false, 10, 1000 * 30, 5));

    CIM::IOManager::GetThis()->addTimer(1000, [pool]()
                                          {
            auto r = pool->doGet("/", 300);
            CIM_LOG_INFO(g_logger) << r->toString(); }, true);
}

void run()
{
    // CIM::Address::ptr addr = CIM::Address::LookupAnyIpAddress("www.CIM.top:80");
    // if (!addr)
    // {
    //     CIM_LOG_INFO(g_logger) << "get addr error";
    //     return;
    // }

    // CIM::Socket::ptr sock = CIM::Socket::CreateTCP(addr);
    // bool rt = sock->connect(addr);
    // if (!rt)
    // {
    //     CIM_LOG_INFO(g_logger) << "connect " << *addr << " failed";
    //     return;
    // }

    // CIM::http::HttpConnection::ptr conn(new CIM::http::HttpConnection(sock));
    // CIM::http::HttpRequest::ptr req(new CIM::http::HttpRequest);
    // req->setPath("/blog/");
    // req->setHeader("host", "www.CIM.top");
    // CIM_LOG_INFO(g_logger) << "req:" << std::endl
    //                          << *req;

    // conn->sendRequest(req);
    // auto rsp = conn->recvResponse();

    // if (!rsp)
    // {
    //     CIM_LOG_INFO(g_logger) << "recv response error";
    //     return;
    // }
    // CIM_LOG_INFO(g_logger) << "rsp:" << std::endl
    //                          << *rsp;

    // std::ofstream ofs("rsp.dat");
    // ofs << *rsp;

    // CIM_LOG_INFO(g_logger) << "=========================";

    // auto r = CIM::http::HttpConnection::DoGet("http://www.baidu.com", 300);
    // CIM_LOG_INFO(g_logger) << "result=" << r->result
    //                          << " error=" << r->error
    //                          << " rsp=" << (r->response ? r->response->toString() : "");

    // CIM_LOG_INFO(g_logger) << "=========================";
    test_pool();
}

void test_https()
{
    auto r = CIM::http::HttpConnection::DoGet("http://www.baidu.com/", 300, {{"Accept-Encoding", "gzip, deflate, br"}, {"Connection", "keep-alive"}, {"User-Agent", "curl/7.29.0"}});
    CIM_LOG_INFO(g_logger) << "result=" << r->result
                             << " error=" << r->error
                             << " rsp=" << (r->response ? r->response->toString() : "");

    // CIM::http::HttpConnectionPool::ptr pool(new CIM::http::HttpConnectionPool(
    //             "www.baidu.com", "", 80, false, 10, 1000 * 30, 5));
    auto pool = CIM::http::HttpConnectionPool::Create(
        "https://www.baidu.com", "", 10, 1000 * 30, 5);
    CIM::IOManager::GetThis()->addTimer(1000, [pool]()
                                          {
            auto r = pool->doGet("/", 3000, {
                        {"Accept-Encoding", "gzip, deflate, br"},
                        {"User-Agent", "curl/7.29.0"}
                    });
            CIM_LOG_INFO(g_logger) << r->toString(); }, true);
}

void test_data()
{
    CIM::Address::ptr addr = CIM::Address::LookupAny("www.baidu.com:80");
    auto sock = CIM::Socket::CreateTCP(addr);

    sock->connect(addr);
    const char buff[] = "GET / HTTP/1.1\r\n"
                        "connection: close\r\n"
                        "Accept-Encoding: gzip, deflate, br\r\n"
                        "Host: www.baidu.com\r\n\r\n";
    sock->send(buff, sizeof(buff));

    std::string line;
    line.resize(1024);

    std::ofstream ofs("http.dat", std::ios::binary);
    int total = 0;
    int len = 0;
    while ((len = sock->recv(&line[0], line.size())) > 0)
    {
        total += len;
        ofs.write(line.c_str(), len);
    }
    std::cout << "total: " << total << " tellp=" << ofs.tellp() << std::endl;
    ofs.flush();
}

void test_parser()
{
    std::ifstream ifs("http.dat", std::ios::binary);
    std::string content;
    std::string line;
    line.resize(1024);

    int total = 0;
    while (!ifs.eof())
    {
        ifs.read(&line[0], line.size());
        content.append(&line[0], ifs.gcount());
        total += ifs.gcount();
    }

    std::cout << "length: " << content.size() << " total: " << total << std::endl;
    CIM::http::HttpResponseParser parser;
    size_t nparse = parser.execute(&content[0], content.size(), false);
    std::cout << "finish: " << parser.isFinished() << std::endl;
    content.resize(content.size() - nparse);
    std::cout << "rsp: " << *parser.getData() << std::endl;

    auto &client_parser = parser.getParser();
    std::string body;
    int cl = 0;
    do
    {
        size_t nparse = parser.execute(&content[0], content.size(), true);
        std::cout << "content_len: " << client_parser.content_len
                  << " left: " << content.size()
                  << std::endl;
        cl += client_parser.content_len;
        content.resize(content.size() - nparse);
        body.append(content.c_str(), client_parser.content_len);
        content = content.substr(client_parser.content_len + 2);
    } while (!client_parser.chunks_done);

    std::cout << "total: " << body.size() << " content:" << cl << std::endl;

    CIM::ZlibStream::ptr stream = CIM::ZlibStream::CreateGzip(false);
    stream->write(body.c_str(), body.size());
    stream->flush();

    body = stream->getResult();

    std::ofstream ofs("http.txt");
    ofs << body;
}

int main(int argc, char **argv)
{
    CIM::IOManager iom(2);
    // iom.schedule(run);
    iom.schedule(run);
    return 0;
}