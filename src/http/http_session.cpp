#include "http/http_session.hpp"

#include "http/http_parser.hpp"

namespace CIM::http {
HttpSession::HttpSession(Socket::ptr sock, bool owner) : SocketStream(sock, owner) {}

HttpRequest::ptr HttpSession::recvRequest() {
    // 创建HTTP请求解析器实例，用于解析接收到的数据
    HttpRequestParser::ptr parser(new HttpRequestParser);
    // 获取HTTP请求缓冲区大小配置，用于控制每次读取数据的大小（防止恶意数据）
    uint64_t buff_size = HttpRequestParser::GetHttpRequestBufferSize();
    // uint64_t buff_size = 100;
    // 分配缓冲区内存，使用智能指针管理内存自动释放
    std::shared_ptr<char> buffer(new char[buff_size], [](char* ptr) { delete[] ptr; });
    char* data = buffer.get();
    int offset = 0;  // 记录缓冲区中未处理数据的偏移量

    // 循环读取数据直到解析完成或出错
    do {
        // 从socket中读取数据，读取大小为缓冲区剩余空间大小
        int len = read(data + offset, buff_size - offset);
        // 如果读取失败或连接关闭，则关闭会话并返回空指针
        if (len <= 0) {
            close();
            return nullptr;
        }

        // 计算缓冲区中总的数据长度
        len += offset;
        // 执行解析操作，返回已解析的数据长度
        size_t nparse = parser->execute(data, len);
        // 如果解析过程中出现错误，则关闭会话并返回空指针
        if (parser->hasError()) {
            close();
            return nullptr;
        }

        // 更新偏移量，保留未解析的数据供下次处理
        offset = len - nparse;
        // 如果缓冲区已满但还未解析完成，说明请求过大，关闭连接
        if (offset == (int)buff_size) {
            close();
            return nullptr;
        }
        // 如果解析完成（请求头解析完毕），则跳出循环
        if (parser->isFinished()) {
            break;
        }
    } while (true);

    // 获取请求体长度
    int64_t length = parser->getContentLength();
    // 如果存在请求体数据
    if (length > 0) {
        // 创建存储请求体的字符串
        std::string body;
        body.resize(length);  // 调整字符串大小以容纳请求体数据

        int len = 0;  // 已读取的请求体长度
        // 如果请求体长度大于等于缓冲区中已有的数据量
        if (length >= offset) {
            // 将缓冲区中的数据复制到请求体中
            memcpy(&body[0], data, offset);
            len = offset;  // 更新已读取长度
        } else {
            // 只复制需要的部分数据
            memcpy(&body[0], data, length);
            len = length;  // 更新已读取长度
        }

        // 计算剩余需要读取的请求体长度
        length -= offset;
        // 如果还有剩余数据需要读取
        if (length > 0) {
            // 读取固定大小的数据到请求体剩余部分
            if (readFixSize(&body[len], length) <= 0) {
                // 如果读取失败，则关闭连接并返回空指针
                close();
                return nullptr;
            }
        }
        // 设置解析得到的HTTP请求对象的请求体
        parser->getData()->setBody(body);
    }

    // 初始化HTTP请求对象（解析请求头中的信息）
    parser->getData()->init();
    // 返回解析得到的HTTP请求对象
    return parser->getData();
}

int HttpSession::sendResponse(HttpResponse::ptr rsp) {
    std::stringstream ss;
    ss << *rsp;
    std::string data = ss.str();
    return writeFixSize(data.c_str(), data.size());
}
}  // namespace CIM::http