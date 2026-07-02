# 里程碑 06：HTTPS/TLS

> **目标**：添加 TLS 加密，实现 HTTPS。

## 前置

完成里程碑 04（需要先有稳定的多线程模型）

## 任务

1. 用 OpenSSL 库集成 TLS
2. 生成自签名证书
3. 支持同时监听 HTTP 和 HTTPS 端口
4. 客户端用 `curl https://... -k` 验证

## 关键概念

- TLS 握手过程
- 证书链（CA/中间证书/服务端证书）
- OpenSSL 的 BIO 抽象层
