#include "http/ws_session.hpp"

#include <string.h>

#include "base/endian.hpp"
#include "base/macro.hpp"
#include "util/hash_util.hpp"

namespace CIM::http {
static CIM::Logger::ptr g_logger = CIM_LOG_NAME("system");

// 是否允许客户端未掩码帧（仅用于兼容非标准客户端，默认关闭）
static CIM::ConfigVar<uint32_t>::ptr g_ws_allow_unmasked_client_frames =
    CIM::Config::Lookup("websocket.allow_unmasked_client_frames", (uint32_t)0,
                        "allow unmasked websocket frames from client side");

CIM::ConfigVar<uint32_t>::ptr g_websocket_message_max_size = CIM::Config::Lookup(
    "websocket.message.max_size", (uint32_t)1024 * 1024 * 32, "websocket message max size");

WSSession::WSSession(Socket::ptr sock, bool owner) : HttpSession(sock, owner) {}

HttpRequest::ptr WSSession::handleShake() {
    HttpRequest::ptr req;
    do {
        req = recvRequest();
        if (!req) {
            CIM_LOG_INFO(g_logger) << "invalid http request";
            break;
        }
        // Upgrade: websocket（大小写不敏感）
        if (strcasecmp(req->getHeader("Upgrade").c_str(), "websocket")) {
            CIM_LOG_INFO(g_logger) << "http header Upgrade != websocket";
            break;
        }
        // Connection 头可能包含多值，如 "keep-alive, Upgrade"，这里放宽为包含 Upgrade 即可
        {
            std::string conn = req->getHeader("Connection");
            std::string conn_lc = conn;
            for (auto& c : conn_lc) c = ::tolower(c);
            if (conn_lc.find("upgrade") == std::string::npos) {
                CIM_LOG_INFO(g_logger) << "http header Connection not contains Upgrade, got: "
                                       << conn;
                break;
            }
        }
        if (req->getHeaderAs<int>("Sec-webSocket-Version") != 13) {
            CIM_LOG_INFO(g_logger) << "http header Sec-webSocket-Version != 13";
            break;
        }
        std::string key = req->getHeader("Sec-WebSocket-Key");
        if (key.empty()) {
            CIM_LOG_INFO(g_logger) << "http header Sec-WebSocket-Key = null";
            break;
        }

        std::string v = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        v = CIM::base64encode(CIM::sha1sum(v));
        req->setWebsocket(true);

        auto rsp = req->createResponse();
        rsp->setStatus(HttpStatus::SWITCHING_PROTOCOLS);
        rsp->setWebsocket(true);
        rsp->setReason("Web Socket Protocol Handshake");
        rsp->setHeader("Upgrade", "websocket");
        rsp->setHeader("Connection", "Upgrade");
        rsp->setHeader("Sec-WebSocket-Accept", v);

        sendResponse(rsp);
        CIM_LOG_DEBUG(g_logger) << *req;
        CIM_LOG_DEBUG(g_logger) << *rsp;
        return req;
    } while (false);
    if (req) {
        CIM_LOG_INFO(g_logger) << *req;
    }
    return nullptr;
}

WSFrameMessage::WSFrameMessage(int opcode, const std::string& data)
    : m_opcode(opcode), m_data(data) {}

std::string WSFrameHead::toString() const {
    std::stringstream ss;
    ss << "[WSFrameHead fin=" << fin << " rsv1=" << rsv1 << " rsv2=" << rsv2 << " rsv3=" << rsv3
       << " opcode=" << opcode << " mask=" << mask << " payload=" << payload << "]";
    return ss.str();
}

WSFrameMessage::ptr WSSession::recvMessage() {
    return WSRecvMessage(this, false);
}

int32_t WSSession::sendMessage(WSFrameMessage::ptr msg, bool fin) {
    return WSSendMessage(this, msg, false, fin);
}

int32_t WSSession::sendMessage(const std::string& msg, int32_t opcode, bool fin) {
    return WSSendMessage(this, std::make_shared<WSFrameMessage>(opcode, msg), false, fin);
}

int32_t WSSession::ping() {
    return WSPing(this);
}

WSFrameMessage::ptr WSRecvMessage(Stream* stream, bool client) {
    int opcode = 0;
    std::string data;
    int cur_len = 0;
    do {
        // 按字节读取帧头（2字节）
        uint8_t b1 = 0, b2 = 0;
        if (stream->readFixSize(&b1, 1) <= 0) break;
        if (stream->readFixSize(&b2, 1) <= 0) break;

        WSFrameHead ws_head;  // 仅用于日志展示
        ws_head.fin = (b1 & 0x80) != 0;
        ws_head.rsv1 = (b1 & 0x40) != 0;
        ws_head.rsv2 = (b1 & 0x20) != 0;
        ws_head.rsv3 = (b1 & 0x10) != 0;
        ws_head.opcode = (b1 & 0x0F);
        ws_head.mask = (b2 & 0x80) != 0;
        ws_head.payload = (b2 & 0x7F);

        CIM_LOG_DEBUG(g_logger) << "WSFrameHead " << ws_head.toString();

        if (ws_head.opcode == WSFrameHead::PING) {
            CIM_LOG_INFO(g_logger) << "PING";
            if (WSPong(stream) <= 0) break;
            continue;
        } else if (ws_head.opcode == WSFrameHead::PONG) {
            // 忽略
            continue;
        } else if (ws_head.opcode == WSFrameHead::CONTINUE ||
                   ws_head.opcode == WSFrameHead::TEXT_FRAME ||
                   ws_head.opcode == WSFrameHead::BIN_FRAME) {
            if (!client && !ws_head.mask) {
                if (!g_ws_allow_unmasked_client_frames->getValue()) {
                    CIM_LOG_WARN(g_logger)
                        << "Unmasked WebSocket frame from client, closing connection (enforce RFC6455)";
                    // 依据RFC6455返回 1002(Protocol Error) 后再关闭
                    WSClose(stream, 1002, "Client must mask frames");
                    break;
                } else {
                    CIM_LOG_WARN(g_logger)
                        << "Unmasked WebSocket frame from client, allowed by config (compat mode)";
                    // 兼容模式：不中断，继续按未掩码处理
                }
            }

            uint64_t length = 0;
            if (ws_head.payload == 126) {
                uint16_t len = 0;
                if (stream->readFixSize(&len, sizeof(len)) <= 0) break;
                length = CIM::byteswap(len);
            } else if (ws_head.payload == 127) {
                uint64_t len = 0;
                if (stream->readFixSize(&len, sizeof(len)) <= 0) break;
                length = CIM::byteswap(len);
            } else {
                length = ws_head.payload;
            }

            if ((cur_len + length) >= g_websocket_message_max_size->getValue()) {
                CIM_LOG_WARN(g_logger)
                    << "WSFrameMessage length > " << g_websocket_message_max_size->getValue()
                    << " (" << (cur_len + length) << ")";
                break;
            }

            char mask_key[4] = {0};
            if (ws_head.mask) {
                if (stream->readFixSize(mask_key, sizeof(mask_key)) <= 0) break;
            }

            data.resize(cur_len + length);
            if (stream->readFixSize(&data[cur_len], length) <= 0) break;
            if (ws_head.mask) {
                for (uint64_t i = 0; i < length; ++i) {
                    data[cur_len + i] ^= mask_key[i % 4];
                }
            }
            cur_len += length;

            if (!opcode && ws_head.opcode != WSFrameHead::CONTINUE) {
                opcode = ws_head.opcode;
            }

            if (ws_head.fin) {
                CIM_LOG_DEBUG(g_logger) << data;
                return WSFrameMessage::ptr(new WSFrameMessage(opcode, std::move(data)));
            }
        } else {
            CIM_LOG_DEBUG(g_logger) << "invalid opcode=" << ws_head.opcode;
        }
    } while (true);
    stream->close();
    return nullptr;
}

int32_t WSSendMessage(Stream* stream, WSFrameMessage::ptr msg, bool client, bool fin) {
    do {
        uint64_t size = msg->getData().size();

        // 首字节：FIN/RSV/OPCODE
        uint8_t b1 = 0;
        if (fin) b1 |= 0x80;              // FIN
        b1 |= (msg->getOpcode() & 0x0F);  // OPCODE

        // 次字节：MASK/PAYLOAD LEN (7位)
        uint8_t b2 = 0;
        if (client) b2 |= 0x80;  // 客户端发送必须MASK

        uint8_t len_indicator = 0;
        if (size < 126) {
            len_indicator = (uint8_t)size;
        } else if (size < 65536) {
            len_indicator = 126;
        } else {
            len_indicator = 127;
        }
        b2 |= (len_indicator & 0x7F);

        if (stream->writeFixSize(&b1, 1) <= 0) break;
        if (stream->writeFixSize(&b2, 1) <= 0) break;

        if (len_indicator == 126) {
            uint16_t len = (uint16_t)size;
            len = CIM::byteswap(len);
            if (stream->writeFixSize(&len, sizeof(len)) <= 0) break;
        } else if (len_indicator == 127) {
            uint64_t len = CIM::byteswap(size);
            if (stream->writeFixSize(&len, sizeof(len)) <= 0) break;
        }

        if (client) {
            // 生成掩码并写入掩码后数据
            char mask[4];
            uint32_t rand_value = rand();
            memcpy(mask, &rand_value, sizeof(mask));
            if (stream->writeFixSize(mask, sizeof(mask)) <= 0) break;

            std::string masked = msg->getData();
            for (size_t i = 0; i < masked.size(); ++i) {
                masked[i] ^= mask[i % 4];
            }
            if (stream->writeFixSize(masked.data(), masked.size()) <= 0) break;
            return (int32_t)(2 + (len_indicator == 126 ? 2 : (len_indicator == 127 ? 8 : 0)) + 4 +
                             masked.size());
        } else {
            // 服务端发送不使用掩码
            if (stream->writeFixSize(msg->getData().data(), size) <= 0) break;
            return (int32_t)(2 + (len_indicator == 126 ? 2 : (len_indicator == 127 ? 8 : 0)) +
                             size);
        }
    } while (0);
    stream->close();
    return -1;
}

int32_t WSSession::pong() {
    return WSPong(this);
}

int32_t WSPing(Stream* stream) {
    uint8_t b1 = 0x80 | (uint8_t)WSFrameHead::PING;  // FIN + PING
    uint8_t b2 = 0x00;                               // 无掩码、长度0
    if (stream->writeFixSize(&b1, 1) <= 0) {
        stream->close();
        return -1;
    }
    if (stream->writeFixSize(&b2, 1) <= 0) {
        stream->close();
        return -1;
    }
    return 2;
}

int32_t WSPong(Stream* stream) {
    uint8_t b1 = 0x80 | (uint8_t)WSFrameHead::PONG;  // FIN + PONG
    uint8_t b2 = 0x00;                               // 无掩码、长度0
    if (stream->writeFixSize(&b1, 1) <= 0) {
        stream->close();
        return -1;
    }
    if (stream->writeFixSize(&b2, 1) <= 0) {
        stream->close();
        return -1;
    }
    return 2;
}

int32_t WSClose(Stream* stream, uint16_t code, const std::string& reason) {
    // CLOSE 帧：FIN + OPCODE(CLOSE)
    uint8_t b1 = 0x80 | (uint8_t)WSFrameHead::CLOSE;
    std::string payload;
    payload.resize(2);
    // 状态码为网络字节序
    uint16_t ncode = htons(code);
    memcpy(&payload[0], &ncode, 2);
    if (!reason.empty()) {
        payload.append(reason);
    }

    uint8_t b2 = 0x00;  // 服务端发给客户端不掩码
    size_t size = payload.size();
    uint8_t len_indicator = 0;
    if (size < 126) {
        len_indicator = (uint8_t)size;
        b2 |= (len_indicator & 0x7F);
        if (stream->writeFixSize(&b1, 1) <= 0) goto fail;
        if (stream->writeFixSize(&b2, 1) <= 0) goto fail;
    } else if (size < 65536) {
        len_indicator = 126;
        b2 |= (len_indicator & 0x7F);
        if (stream->writeFixSize(&b1, 1) <= 0) goto fail;
        if (stream->writeFixSize(&b2, 1) <= 0) goto fail;
        uint16_t len = CIM::byteswap((uint16_t)size);
        if (stream->writeFixSize(&len, sizeof(len)) <= 0) goto fail;
    } else {
        len_indicator = 127;
        b2 |= (len_indicator & 0x7F);
        if (stream->writeFixSize(&b1, 1) <= 0) goto fail;
        if (stream->writeFixSize(&b2, 1) <= 0) goto fail;
        uint64_t len = CIM::byteswap((uint64_t)size);
        if (stream->writeFixSize(&len, sizeof(len)) <= 0) goto fail;
    }
    if (size > 0) {
        if (stream->writeFixSize(payload.data(), payload.size()) <= 0) goto fail;
    }
    return (int32_t)(2 + (len_indicator == 126 ? 2 : (len_indicator == 127 ? 8 : 0)) + size);

fail:
    stream->close();
    return -1;
}
}  // namespace CIM::http