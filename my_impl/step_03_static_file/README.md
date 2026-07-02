# 里程碑 03：静态文件服务

> **目标**：从磁盘读取文件，通过 HTTP 返回 — 让浏览器能看到 HTML 页面。

## 前置

完成里程碑 02

## 任务

1. 指定一个文档根目录（如 `www/`）
2. 当请求 `/index.html` 时，读取 `www/index.html` 并返回
3. 正确设置 `Content-Type`（至少支持 html, css, js, png, jpg）
4. 文件不存在时返回 `404`
5. 请求 `/` 时自动补 `index.html`
