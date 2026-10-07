# RT-Thread HTTP Transport

公共头 `agent_rtthread_transport.h` 不含 SDK 类型；实现使用 `webclient.h` 2.3 API。

```c
static agent_port_rtthread_transport_t http_state;
static char url[256], io[512];
static agent_http_header_t headers[24];
agent_transport_t transport;
agent_port_rtthread_transport_config_t config = {0};
config.runtime = runtime;  /* 与 Core/Provider 使用同一已初始化时钟。 */
config.sdk_header_capacity = 4096;
config.url_buffer = url;
config.url_capacity = sizeof(url);
config.io_buffer = io;
config.io_capacity = sizeof(io);
config.response_headers = headers;
config.response_header_capacity = sizeof(headers) / sizeof(headers[0]);
config.authenticated_tls = application_verified_tls_backend;
agent_error_t error = agent_port_rtthread_transport_init(&transport, &http_state, &config);
```

`application_verified_tls_backend` 是应用对 TLS 后端确实验证 **CA 链和目标主机名**
的确认，不是关闭验证的开关。适配器不设置私有 SSL 对象，布尔值不能替应用证明
安全性。HTTPS 无编译的 TLS 后端返回 NOT_SUPPORTED，无安全确认返回 AUTH。
应用应使用 HTTPS 传凭证；初始化不连接或探测服务。

## 链路与契约

```text
Provider -> transport.ops->request
  -> 校验 POST / URL / headers / buffer alias / SDK 请求头容量
  -> 创建 WebClient session -> 受控请求头 -> webclient_post
  -> 校验响应头与 framing -> sink.headers 一次
  -> webclient_read -> sink.body 零或多次 -> close -> 返回
```

- 仅非空 POST。上游 GET 自动跟随重定向，空 POST 不完整处理响应；不能伪装
  全方法支持。拒绝 3xx，不重试、不跨主机转发凭证。
- 401/403/429/5xx 交给 sink，Provider 决定错误码，不混淆 HTTP 与链路失败。
- URL、I/O、响应头描述符为 caller-owned，互不重叠并活到 request 返回。
  同一实例须串行，回调重入 BUSY；init/deinit 配置也不得与 request 并发。
- 回调视图仅当次有效，返回前所有回调完成；首个 sink 错误停止投递，始终关闭
  session。无长期 SDK 句柄，不需要 destroy。
- SDK header allocation 为 1..65535 字节，请求及 SDK 拼接文本须能放入。
  响应同时受 SDK、请求 header 字节预算与描述符容量限制。
- 支持 Content-Length、SDK 解码的 chunked、connection-close；拒绝冲突 framing、
  重复 Content-Length、SDK 不识别的 Transfer-Encoding 和压缩响应。发送
  `Connection: close`、`Accept-Encoding: identity`；正文累计不得超预算。
- 拒绝 CR/LF/NUL、非法 header name、重复请求字段、URL userinfo/fragment。
  Host/Content-Length/Transfer-Encoding/Connection/Expect/Accept-Encoding 由
  Adapter 控制，不能由请求覆盖。

## 限制

取消和绝对 deadline 在 SDK 调用前后及回调间检查，**不能打断 SDK 内部 DNS、
connect、TLS handshake、send/recv 阻塞**；不承诺精确墙钟截止。SDK I/O timeout
与网络栈超时由产品配置，没有用配置字段虚假宣称限定整个建连阶段。

核对的上游 POST 忽略部分 `webclient_write` 返回值，本层无法恢复已经丢失的错误。
上游 header/chunk parser 也先于本层校验运行，有界后验检查不能代替 SDK 审计。
产品需固定并审核 WebClient/TLS 版本；若需可中断 I/O 或更强 framing 保证，注入
另一实现的统一 Transport Ops。编译时阻止 `WEBCLIENT_DEBUG`，避免凭证日志。

Core 零 heap 不等于 SDK 零 heap。session/header/连接/TLS 内存由平台管理，
调用方 buffer 不是整个联网峰值；板上仍需测峰值、栈、超时和证书拒绝。
