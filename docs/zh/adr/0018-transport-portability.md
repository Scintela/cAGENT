# ADR 0018: HTTP/TLS Transport 的跨平台 Adapter 架构

- 状态：已采纳（同步 HTTP Contract 与目录边界）；ESP-IDF/OpenVela Adapter 已有初版，目标设备验证待完成
- 日期：2026-09-27

## 背景

云端 LLM 通常需要 DNS、TCP、TLS 和 HTTP；不同系统提供的 API 可能是完整 HTTP client、
socket + TLS 库、异步事件 API、AT modem 或私有网络服务。Core 不应依赖这些实现，但
OpenAI-compatible Model Provider 仍需要一个可移植的方式发送请求、读取 HTTP body 与处理取消。

ADR 0007 已提出同步的 `agent_transport_t` callback contract。本 ADR 不替代其中的请求/sink 基本
语义，而是集中讨论跨平台代码放置、TLS 责任、Adapter 选择以及后续需由用户裁决的公开边界。

## 不变量

```text
Agent Core
  -> Model Provider
      -> HTTP Transport Adapter
          -> DNS / TCP / TLS / platform network stack
```

- Core 只调用 Model Provider，不构造 HTTP、TLS 或厂商 JSON。
- Model Provider 拥有厂商 endpoint、认证 header、请求/响应 JSON、SSE 与 HTTP status 语义。
- Transport 负责 HTTP framing、响应交付、deadline/cancel、DNS/socket/TLS 与其平台资源。
- HTTP status 和 body 必须交给 Model Provider；Transport 的 `AGENT_OK` 只表示交换和 sink 交付完成，
  不表示 HTTP 2xx。
- TLS 证书、连接池、DNS cache、socket、网络 task 栈和 I/O buffer 都不属于 `agent_workspace_t`。

## 统一 Ops 与实例绑定

`include/agent/transport.h` 定义统一 HTTP Contract：`agent_http_request_t` 表示一次有界请求，
`agent_http_sink_t` 接收状态码、响应头及分块 body，`agent_transport_ops_t.request` 执行同步交换。
`agent_transport_t` 仅保存不可变 ops 表指针和平台实例 `context`，本身不拥有该实例。
`src/transport/transport.c` 提供平台无关的参数校验与转发；其调用入口目前是内部接口，
不是 Agent Core 持有的全局 HTTP 服务。

ESP-IDF Adapter 通过 `agent_port_espidf_transport_init(out, state, config)` 把自己的
`request` 实现绑定到 `out->ops`，并把调用方持有的状态放入 `out->context`。应用初始化后，
联网 Model Provider 在其专属配置中借用该 Transport，Agent 再绑定 Model；
`agent_config_t` 不含 Transport 字段。OpenAI Provider 已实现同步非流式 Chat Completions
请求与响应转换，并通过专属配置借用 Transport；配置及缓冲区契约见
[`providers/openai/README.md`](../../../providers/openai/README.md)。Core 的 `agent_run()`
仍未实现完整 ReAct 链路，不能把 Provider 可用误认为端到端 Agent 对话已经可用。

## 方案比较

### 方案 A：Model Provider 直接调用平台 HTTP SDK

OpenAI Provider 直接调用 `esp_http_client`、NuttX socket 或 RT-Thread webclient。

| 优点 | 代价 |
|---|---|
| 单平台原型最少层次。 | Provider 无法跨平台复用，SDK 类型渗入 Model 代码。 |
| 可直接使用 SDK 的流式和连接池能力。 | Mock、Host 与多个 MCU 需要重复实现同一 Model 协议。 |

仅适合明确声明为平台专用 addon，不适合通用 cAgent Provider。

### 方案 B：通用 HTTP contract + 平台 Adapter

Model Provider 注入 `agent_transport_t`，平台 Adapter 实现一次同步有界 HTTP 交换。

| 优点 | 代价 |
|---|---|
| Model Provider 可复用于 ESP-IDF、OpenVela、RT-Thread、Host 与 Mock。 | 每个平台都要实现 deadline、cancel 和 sink 生命周期。 |
| Core、Provider 与 SDK 依赖清晰可裁剪。 | 通用 API 必须避免过早加入所有 HTTP 特性。 |
| 可让应用实现自己的 Transport。 | Adapter 的 buffer/连接池内存需单独测量。 |

这是已采纳的首版基础。

### 方案 C：Core 内置 socket/TLS 抽象

Transport 通用层先定义 socket、DNS 与 TLS 的统一 ops，再在其上实现 HTTP。

| 优点 | 代价 |
|---|---|
| 可在多个 Provider 之间复用低层连接抽象。 | API 面显著扩大，要处理证书、SNI、IPv4/IPv6、代理、ALPN、连接复用与异步状态。 |
| 对 socket + mbedTLS 平台看似统一。 | 完整 HTTP SDK 平台反而需要被降级适配；首版 MCU 成本过高。 |

不建议作为首版通用 ABI。将来确有多个协议共享低层连接需求时再单独设计。

### 方案 D：统一第三方 HTTP 库

全部目标使用 libcurl、Mongoose 或单一 socket/TLS 组合。

优点是测试面小；缺点是 ROM/RAM、许可证、TLS 配置和平台网络集成往往不适合 MCU，且会放弃
ESP-IDF/NuttX/RT-Thread 已有能力。可作为 Host 或特定产品 Adapter 的内部实现，不能成为 Core 要求。

### 方案 E：公开异步 `start/poll/cancel` Transport

可减少阻塞任务时间，但需要请求句柄、回调排空、跨任务生命周期、背压、buffer 所有权和 Core
状态机配合。首版不建议；同步 contract 应先实现并测量。异步 SDK 可由 Adapter 内部等待或轮询，
但必须在 `request()` 返回前停止一切 sink callback。

## HTTP/TLS 的责任边界

| 能力 | Model Provider | Transport Adapter | 平台网络栈 |
|---|---:|---:|---:|
| OpenAI JSON、SSE、Tool call | 是 | 否 | 否 |
| URL endpoint、Authorization header | 是 | 传输，不解释 | 否 |
| HTTP method/header/body framing | 构造请求语义 | 发送/解码 | 可由 SDK 代管 |
| HTTP status 401/429/5xx 解释 | 是 | 原样交付 | 否 |
| DNS、TCP、TLS handshake、证书验证 | 否 | 是或委托 SDK | 实际执行 |
| 连接池、重连、TLS session cache | Provider 策略或应用 | 实现/配置 | 可由 SDK 代管 |
| API key、证书、私钥 | Provider/应用配置 | 只按配置使用 | 安全硬件可实际保存 |

Transport 不应把 HTTP `401`、`429`、`5xx` 直接转换为 `AGENT_ERROR_IO`；它必须先完整交付 status
和 body。Model Provider 再映射为 `AGENT_ERROR_AUTH`、`AGENT_ERROR_MODEL_RATE_LIMIT`、
`AGENT_ERROR_MODEL_UNAVAILABLE` 等。

Transport 自己的失败映射为：DNS/socket/TLS 失败为 `IO`，deadline 为 `TIMEOUT`，取消为
`CANCELLED`，固定 response/staging buffer 耗尽为 `CAPACITY`，不支持的 scheme/TLS 能力为
`NOT_SUPPORTED`。平台 native code 仅保留在 Adapter 私有日志或诊断中。

## TLS 实现选择

| 选择 | 适用系统 | 优点 | 代价 |
|---|---|---|---|
| 平台 HTTP SDK 管理 TLS | ESP-IDF 或有完整 client 的系统 | 最少适配代码，证书和连接复用可用 SDK。 | Adapter 受 SDK 回调/heap 行为约束。 |
| Adapter 组合 socket + mbedTLS | OpenVela/NuttX、裁剪 RTOS | 可控 buffer、证书和 PSRAM 放置。 | TLS 状态、SNI、证书链和回调排空由 Adapter 负责。 |
| 应用提供私有 Transport | AT modem、网关、企业网络库 | 最适合已有网络栈。 | 应用承担完整 contract 测试。 |

三种选择都能实现相同 `agent_transport_t`；Core 和通用 Model Provider 不应知道具体选项。
首版不增加公共 TLS VTable，也暂不把 CA 来源、证书格式等做成统一的
`agent_tls_config_t`。由各 Adapter 的实例配置处理证书、信任根及平台特性；
HTTPS 必须验证服务器证书链和目标主机名，缺少可信验证条件时应失败，不能静默降级。
当前 ESP-IDF Adapter 在 HTTPS 请求前要求 PEM CA 或编译期开启的证书 Bundle，缺失时直接失败；
OpenVela `webclient` Adapter 要求应用提供 TLS ops，无法自行证明该实现已验证证书链和主机名。
两者都仍需目标平台集成及安全测试，不应把 Mock 测试视作 TLS 验证。

## 目录与构建方案

### 候选一：Adapter 留在 `src/transport/`

```text
src/transport/transport.c
src/transport/transport_espidf.c
src/transport/transport_openvela.c
```

优点是目录集中；缺点是主库构建系统必须持续处理多个 SDK 的条件源文件。适合单仓库、统一 Kconfig
项目，不太适合被其他构建系统以源码方式嵌入。

### 采用：独立 `ports/` 包

```text
src/transport/                         # generic validation and internal dispatch
ports/espidf/transport/include/agent_espidf_transport.h
ports/espidf/transport/src/transport.c
ports/openvela/transport/include/agent_openvela_transport.h
ports/openvela/transport/src/transport.c
ports/rtthread/include/agent_rtthread_transport.h
ports/rtthread/transport/src/transport.c
ports/host/include/agent_host_transport.h
ports/host/transport/src/transport.c
```

平台依赖和 Kconfig/CMake 配置不污染 Core；代价是发布与版本协同更复杂。对跨系统开源库更合适，并
与 ADR 0017 的 Runtime Port 采用同一组织模型。目前已有 ESP-IDF 和基于 NuttX
`netutils/webclient` 的 OpenVela Adapter 初版；其他未实现的 Adapter 不导出占位 API，
也不进入任何默认构建。

构建期选择的是参与编译的 Backend 及其 SDK 依赖，初始化期选择的是具体
`agent_transport_t` 实例。单平台产品通常只编译一个官方 Backend，可由 Port 的 Kconfig/CMake
提供默认选项；公共 Contract 不规定全固件只能有一个 Backend 或一个实例。同一产品可以同时链接
官方 Adapter、应用自定义 Adapter 或 Mock，分别注入不同的 Model/Tool；不联网的产品可一个也不选。
Core 不识别 `CAGENT_HTTP_BACKEND` 之类的全局枚举，也不设置全局 Transport 单例。

### 候选三：完全由应用实现

Core 只发布 `transport.h`，不维护官方 Adapter。维护成本最低，但产品会重复处理 HTTP callback
排空、限额、取消和错误映射。适合作为始终允许的逃生口，不建议是唯一选择。

## 当前公开头的收敛项

`transport.h` 已收敛以下基础 ABI：

1. `agent_transport_t` 永远是 borrowed binding，`agent_transport_ops_t` 不包含 `destroy`；类型化平台
   或 Provider API 自行提供 `deinit`。
2. `headers` 和 `body` sink 都必须非空；不消费的一方传 no-op callback，避免 Adapter 对 NULL 产生不同
   语义。
3. `agent_transport_ops_t` 首版只有同步 `request`；取消通过请求中的 token 协作检查，
   不另设公共 `cancel` VTable 方法。

同时应决定 response header/body 的总字节计数、chunk 顺序、零字节 body、chunked decode、取消后
异步 SDK callback 排空的精确测试规则。ADR 0007 已给出同步 contract 草案，可作为收敛起点。

## 待决项

1. ESP-IDF/OpenVela 已有 mock-SDK 验证的 Adapter；何时完成设备验证并加入 RT-Thread。
2. 首版已选择完整 request body、同步 `request()`；何时增加请求 streaming 或异步接口仍待真实需求。
3. 各 Adapter 如何验证 HTTPS 信任来源、证书格式、超时与取消行为，并测量私有内存。
4. OpenVela `webclient` 自动重定向无法按当前 Contract 原样交付 3xx，因此 Adapter 明确拒绝 3xx；
   若产品需要 3xx 响应或硬截止时间，应选其他 HTTP Backend。其阻塞 DNS/IO 期间无法保证即时取消。

## 验证要求

- Mock、Host 与至少一个 MCU Adapter 对同一 request/sink 契约执行相同测试；
- headers 恰好一次且先于 body；sink 返回错误后没有更多 callback；`request()` 返回后不再 callback；
- 覆盖 DNS、TLS、socket、cancel、deadline、response/staging capacity、HTTP 401/429/5xx 与 chunked body；
- 报告 Adapter state、DNS/TLS、HTTP buffer、连接池和网络 task 栈峰值，不与 Core workspace 混算；
- 未选择 HTTP Adapter 的本地 Model/Mock Profile 不链接 HTTP、TLS 或 DNS 依赖。

## 与 ADR 0007 的关系

ADR 0007 是现有 HTTP request/sink 同步语义的详细提案；本 ADR 聚焦跨平台实现与目录组织。最终应在
上述待决项裁定后合并或将 0007 标记为被 0018 取代，避免两份文档同时定义不一致的 Transport ABI。
