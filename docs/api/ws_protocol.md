# WebSocket 通讯协议规范（前后端统一）

## 连接
- Endpoint: `/wss/default.io`
- Scheme: `ws://` 或 `wss://`
- Query 参数：
  - `token`: 必填，JWT（payload 中 `sub` 为用户 UID，或后端可解析为 uid）
  - `platform`: 选填，`web|pc|app`，默认 `web`

示例：
```
ws://<host>/wss/default.io?token=<JWT>&platform=web
```

## 握手要求
- 必须使用 RFC6455 WebSocket 协议，浏览器/标准库会自动完成掩码与握手头：
  - `Upgrade: websocket`
  - `Connection: Upgrade`
  - `Sec-WebSocket-Version: 13`
  - `Sec-WebSocket-Key: <random>`
- 客户端->服务端的所有数据帧必须“掩码”传输（Masked）。后端默认拒绝未掩码帧：
  - 配置项：`websocket.allow_unmasked_client_frames`（默认 0=关闭兼容）

## 消息封装
- 统一采用 JSON 文本帧，结构如下：
```json
{
  "event": "<string>",
  "payload": { /* 任意 JSON 对象 */ },
  "ackid": "<string, 可选>"
}
```

### 内置事件
- `connect`（下行）：连接建立成功后由服务端推送。
  - payload: `{"uid": number, "platform": string, "ts": number}`
- `event_error`（下行）：鉴权失败/非法请求。
  - payload: `{"error_code": number, "error_message": string}`
- `ping`（上行）：应用层心跳（浏览器无法发送控制帧）。
- `pong`（下行）：心跳响应，payload: `{"ts": number}`
- `ack`（上行）：收到下行带有 `ackid` 的消息后，客户端回执：
  - 示例：`{"event":"ack","ackid":"<id>"}`

## 心跳
- 客户端每 `20s` 发送一次：`{"event":"ping"}`
- 服务端立即回：`{"event":"pong","payload":{"ts":<ms>}}`

## 推送/业务事件
- 服务端统一通过 `event` 区分：如 `im.message`, `im.message.keyboard`, `im.message.revoke` 等。
- 客户端按事件名注册回调并处理。

## 错误处理
- 鉴权失败：下发 `event_error` 后关闭连接。
- 非法帧/未掩码帧（默认）：直接关闭连接并记录日志。

## 版本与扩展
- Module: `ws.gateway` v0.1.0
- 兼容模式由配置控制：`websocket.allow_unmasked_client_frames: 0|1`
  - 开发/联调可短期开启，生产务必关闭，保证一致性与安全性。
