# HTTP Transport 接口

公共头：`agent/transport.h`。统一 Ops 表为 `agent_transport_ops_t`，
目前只有同步 request；`agent_transport_t` 是借用的 ops + context。
Core config 不绑定 HTTP，Model Provider 持有 Transport。

## 请求与接收

| 类型 | 字段 |
|---|---|
| agent_http_header_t | 长度限定 name/value，不含 CR/LF/NUL |
| agent_http_request_t | method、绝对 URL、headers、body、响应字节/头预算、deadline、cancel |
| agent_http_sink_t | headers、body 和 context，同步消费临时数据 |

body 为字节数据，不要求 UTF-8。响应字节和头预算必须非零。
request 描述符、输入和 sink 状态借用到 request 返回。

## 后端契约

1. 返回前完成所有回调；不能把同步接口暗中改为异步。
2. 最终 headers 在 body 前投递一次；失败若尚无响应，不要求伪造 headers。
3. body 为去除 HTTP transfer framing 后的分块字节，不保证 JSON/SSE 帧边界。
4. sink 首错后停止并传播；不得继续触碰接收缓冲。
5. AGENT_OK 表示 HTTP exchange 成功完成，不自动表示状态码为 2xx。
6. 响应体/头超过预算失败，不静默丢弃数据。
7. 检查取消/deadline；SDK 阻塞阶段能否立即中断取决于后端。
8. HTTPS 验证证书链与主机名，无安全降级。

HTTP 状态码的模型业务映射属于 Model Provider；SSE/JSON 解析也属于协议实现，
不是平台 Port。网络 I/O、TLS 库、证书及内部内存由具体后端负责。

当前没有公共 TCP/TLS/WebSocket VTable，也没有统一 TLS 配置结构。
平台专属配置在 Port 公共头中，不向通用头暴露 SDK 类型。

同一平台可以构建多个后端供不同实例选择，应用也可以仅选择一个。
Ops 接口保持统一，构建选项不应替代实例绑定与所有权。
