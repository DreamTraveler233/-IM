#pragma once

#include "http.hpp"
#include "streams/socket_stream.hpp"

namespace CIM::http {
/**
     * @brief HTTPSession封装
     */
class HttpSession : public SocketStream {
   public:
    /// 智能指针类型定义
    typedef std::shared_ptr<HttpSession> ptr;

    /**
         * @brief 构造函数
         * @param[in] sock Socket类型
         * @param[in] owner 是否托管
         */
    HttpSession(Socket::ptr sock, bool owner = true);

    /**
         * @brief 接收HTTP请求
         * @return 返回HttpRequest对象的智能指针
         * @details 该函数负责从Socket中接收HTTP请求数据，并将其解析为HttpRequest对象
         *          包括解析HTTP请求行、请求头和请求体
         */
    HttpRequest::ptr recvRequest();

    /**
         * @brief 发送HTTP响应
         * @param[in] rsp HTTP响应
         * @return >0 发送成功
         *         =0 对方关闭
         *         <0 Socket异常
         */
    int sendResponse(HttpResponse::ptr rsp);
};
}  // namespace CIM::http