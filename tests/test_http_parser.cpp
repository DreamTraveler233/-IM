#include "http_parser.hpp"
#include "base/macro.hpp"
#include "iostream"

const char test_request_data[] = "POST / HTTP/1.1\r\n"
                                 "Host: www.baidu.com\r\n"
                                 "Content-Length: 10\r\n\r\n"
                                 "1234567890";
static auto g_logger = CIM_LOG_ROOT();

void test_request()
{
    CIM::http::HttpRequestParser::ptr parser(new CIM::http::HttpRequestParser);
    std::string tmp = test_request_data;
    size_t size = parser->execute(&tmp[0], tmp.size());
    CIM_LOG_INFO(g_logger) << "execute rt=" << size
                             << "has_error=" << parser->hasError()
                             << "has_finished=" << parser->isFinished()
                             << "content_length=" << parser->getContentLength();
    tmp.resize(tmp.size() - size);
    std::cout << parser->getData()->toString() << std::endl;
    std::cout << tmp << std::endl;
}

const char test_response_data[] = "HTTP/1.1 200 OK\r\n"
                                  "Date: Tue, 04 Jun 2019 15:43:56 GMT\r\n"
                                  "Server: Apache\r\n"
                                  "Last-Modified: Tue, 12 Jan 2010 13:48:00 GMT\r\n"
                                  "ETag: \"51-47cf7e6ee8400\"\r\n"
                                  "Accept-Ranges: bytes\r\n"
                                  "Content-Length: 81\r\n"
                                  "Cache-Control: max-age=86400\r\n"
                                  "Expires: Wed, 05 Jun 2019 15:43:56 GMT\r\n"
                                  "Connection: Close\r\n"
                                  "Content-Type: text/html\r\n\r\n"
                                  "<html>\r\n"
                                  "<meta http-equiv=\"refresh\" content=\"0;url=http://www.baidu.com/\">\r\n"
                                  "</html>\r\n";

void test_response()
{
    CIM::http::HttpResponseParser parser;
    std::string tmp = test_response_data;
    size_t s = parser.execute(&tmp[0], tmp.size(), true);
    CIM_LOG_ERROR(g_logger) << "execute rt=" << s
                              << " has_error=" << parser.hasError()
                              << " is_finished=" << parser.isFinished()
                              << " total=" << tmp.size()
                              << " content_length=" << parser.getContentLength()
                              << " tmp[s]=" << tmp[s];

    tmp.resize(tmp.size() - s);

    std::cout << parser.getData()->toString() << std::endl;
    std::cout << tmp << std::endl;
}

int main(int argc, char **argv)
{
    test_request();
    CIM_LOG_INFO(g_logger) << "------------------------------------------";
    test_response();
    return 0;
}