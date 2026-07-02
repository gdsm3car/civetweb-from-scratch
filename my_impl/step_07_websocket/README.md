# 里程碑 07：WebSocket

> **目标**：实现 WebSocket 握手与帧通信。

## 前置

完成里程碑 05

## 任务

1. 理解 WebSocket 的升级握手（Upgrade: websocket）
2. 处理 Sec-WebSocket-Key 的响应计算
3. 解析 WebSocket 数据帧（opcode, mask, payload length）
4. 实现一个简单的 echo server

## 小提示

可以先实现最简单的部分：只支持文本帧（opcode=0x1），只解析 unmasked 帧（服务端发送给客户端的帧不需要 mask）。
