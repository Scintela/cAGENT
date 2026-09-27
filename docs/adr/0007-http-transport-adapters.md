# ADR 0007: 可裁剪 HTTP Transport Adapter

- 状态：提案
- 日期：2026-09-23

## 背景

cAgentV2 需要让 OpenAI-compatible Model Provider 在 ESP-IDF、openvela、RT-Thread 和
Host 上发送 HTTP 请求，但 Core 不应知道具体 HTTP client、TLS、DNS、Wi-Fi、线程模型或
连接池。不同平台的 HTTP API 在以下方面不同：

- 阻塞调用、事件回调、异步提交和轮询的执行模型；
- client/session 的创建、连接复用和销毁方式；
- 一次性 body、read loop 和事件 chunk 的响应交付方式；
- TLS 证书、SNI、安全芯片和平台 heap 的配置方式；
- 取消、超时、DNS、socket、TLS 错误的表达方式。

把平台 HTTP callback 放进 `agent_runtime_t` 会使不联网的本地 Model、串口 Model 和
测试 Mock 也依赖 HTTP。让 OpenAI Provider 直接调用平台 SDK 则失去源码级可移植性。

## 决定

HTTP 是独立的扩展能力，不属于 Runtime，也不由 Agent 直接绑定或拥有。首版采用
**同步语义的单请求 Adapter**。构建期裁剪需要的实现，初始化期绑定具体实例：

```text
Application / platform adapter
    owns agent_transport_t
        borrowed by Model Provider
            borrowed or owned by Agent as part of its Model binding
```

Core 只调用 Model；Model Provider 调用 Transport；Transport 调用平台 HTTP/TLS 实现。
Core 不创建网络任务、不维护连接池、不处理证书，也不解析 HTTP 以上的协议。

### 通用边界

`include/agent/transport.h` 保留与平台无关的以下概念：

- `agent_http_request_t`：method、唯一权威的 URL、headers、已完整准备的 body、绝对
  deadline、cancel token 和最大响应字节数；
- `agent_http_sink_t`：一次最终响应的 headers 和零到多个 body chunk；
- `agent_transport_ops_t.request()`：完成一次有界 HTTP 交换；
- `agent_transport_t`：不可变 ops 表和外部 `context` 的借用绑定。

当前 `agent_transport_ops_t` 仅有同步 `request`，不包含 TLS、`destroy` 或独立的
`cancel` VTable 方法。`src/transport/transport.c` 中的校验与转发函数目前仅供内部使用；
已有 ESP-IDF Adapter 可生成 `agent_transport_t`，但 OpenAI Provider 尚未把它接入完整请求链。

第一版不抽象 `connect/send/read/close`，不公开流句柄，也不在 Core 中加入
`start/poll/cancel` 状态机。OpenAI 请求 body 已在 Provider 中完整生成，故不需要首版
请求体 streaming API。OTA、大文件上传下载和 WebSocket 不属于此契约。

### 同步语义

`request()` 可以阻塞，但必须满足：

1. 在 `request()` 返回前完成所有 sink 调用；返回后不得保留 sink、request 或其任何
   借用 view。
2. sink 调用在发起 `request()` 的 Agent 驱动上下文中串行发生；不得由平台网络任务
   并发修改 turn 状态。
3. `headers()` 恰好在 body 前调用一次，即使 header 数为零；`body()` 可调用零次或多次。
4. sink 首次返回错误后停止交付，并返回该错误或等价的 Agent 错误。
5. `AGENT_OK` 表示 HTTP 交换及响应交付完成，不代表 HTTP status 是 2xx。

HTTP status 和响应 body 由 headers/body sink 交给 Model Provider。Provider 负责处理
OpenAI 的 401、429、5xx、JSON 和 SSE；Transport 只负责 HTTP framing、transfer decode、
header/body 限额以及平台 I/O。

原生阻塞 API 可直接映射到此接口。原生异步 API 由 Adapter 在内部等待、轮询或使用
有界队列后回到调用任务交付 sink。Adapter 在取消或超时后必须排空自身平台回调，确保
不会在 `request()` 返回后调用 Agent sink。

### 错误、取消与重试

Transport 仅返回公共 Agent 错误：

| 场景 | 返回值 |
|------|--------|
| DNS、TCP、TLS、socket 读写失败 | `AGENT_ERROR_IO` |
| deadline 到期 | `AGENT_ERROR_TIMEOUT` |
| cancel token 已请求 | `AGENT_ERROR_CANCELLED` |
| 调用者 sink 拒绝 chunk | sink 的错误值 |
| 不支持的 URL/TLS/平台能力 | `AGENT_ERROR_NOT_SUPPORTED` |

平台 errno、mbedTLS 错误码、HTTP status 不得直接作为公共 API status 返回。Provider 或
Adapter 可以把这些值记入其私有日志和诊断数据。HTTP 非 2xx 是已交付的协议响应，不是
自动的 `AGENT_ERROR_IO`。

Adapter 不自动重试、重定向或重放请求。上层若需要可显式配置 Provider 重试策略；任何
策略必须考虑请求可能已发送和远端副作用可能已发生。

### 所有权与生命周期

`agent_transport_t` 默认是应用持有的 borrowed binding：

- ops 表和 `context` 从 Model Provider 配置完成起有效，直至所有借用它的 Model 和
  Agent 均被销毁或解除绑定；
- 多个 Model/Tool 可借用同一个 Transport，但并发安全、连接池和重连由 Adapter 或
  应用负责；
- Agent 不调用 Transport destroy，也不隐式关闭连接；
- 私有 Transport 若由某个具体 Model Provider 创建，其构造和销毁由该 Provider 的
  专用 API 管理，而不是由通用 Agent API 推断。

因此，通用 `agent_transport_ops_t` 不包含 `destroy`。该字段已从公开 ABI 移除，避免
borrowed binding 却含有不清晰的清理入口。

### 平台 Adapter 与构建裁剪

每个 Adapter 独立构建、独立配置，不进入 Core 静态链接依赖。建议的目标布局为：

```text
include/agent/transport.h
src/transport/                            # generic validation and dispatch only

ports/espidf/transport/include/agent_espidf_transport.h
ports/espidf/transport/src/transport.c
ports/openvela/include/agent_openvela_transport.h
ports/openvela/transport/src/transport.c
ports/rtthread/include/agent_rtthread_transport.h
ports/rtthread/transport/src/transport.c
ports/host/include/agent_host_transport.h
ports/host/transport/src/transport.c
```

构建系统以等价于以下的配置裁剪源文件与其平台依赖：

```text
CONFIG_AGENT_TRANSPORT_ESPIDF=y
CONFIG_AGENT_TRANSPORT_OPENVELA=n
CONFIG_AGENT_TRANSPORT_RTTHREAD=n
CONFIG_AGENT_TRANSPORT_HOST=n
```

配置项是 Port 构建系统的职责，不要求 Core 识别平台 enum 或在运行时选择 backend。
普通单平台产品可只编译一个官方 Adapter；这些开关不构成全库互斥的单选规则，
同一固件可同时链接多个 Adapter 或创建多个实例，并由各 Provider/Tool 分别借用。
产品也可完全不编译库提供的 Adapter，而自行实现 `agent_transport_ops_t`。
上面的配置项是示意名称，不等于现有 Kconfig；目前 ESP-IDF Port 使用
`CONFIG_AGENT_PORT_ESPIDF_TRANSPORT`。未实现或未通过契约测试的 Adapter 不应导出占位 API。

面向 MCU 的 Adapter 应优先提供 caller-storage 路径；其状态类型和具体配置由平台
扩展头定义，而非通用 Core ABI。现有 ESP-IDF 初始化接口是：

```c
agent_error_t agent_port_espidf_transport_init(
    agent_transport_t *out,
    agent_port_espidf_transport_t *state,
    const agent_port_espidf_transport_config_t *config);
```

Host Profile 可以额外提供 heap 便利函数。若未来某 Adapter 引入 `deinit()`，只能由应用在
所有消费者已释放后调用；它不是 `agent_destroy()` 的职责。当前 ESP-IDF Adapter 在每次请求
结束时清理其 HTTP client，没有公开 `deinit()`。

### Provider 注入

OpenAI Provider 等联网 Model 将在自身配置中接收 `const agent_transport_t *`；
相应 Provider 配置尚未实现。`agent_config_t` 不包含 Transport，也不增加
`agent_set_transport()` 或 `agent_set_transport_owned()`：

- 本地 Model、Mock 和串口 Model 不应被强制依赖 HTTP；
- Provider 可选择 HTTP、WebSocket、MQTT 或根本不需要网络；
- 连接、证书和重连是 Provider/Adapter 的资源，而非 Agent Core 的全局服务。

需要联网的 Tool 可以在其 `user_data` 中借用相同 Transport；Tool 的网络行为仍由
Tool 作者负责，Core 不因 Tool 使用 HTTP 而产生新的网络生命周期。

## 当前头文件的收敛项

`transport.h` 已完成以下 ABI 收敛；内部 dispatcher 在调用 Adapter 前执行相应参数校验：

1. 从 `agent_transport_ops_t` 删除 `destroy`；通用 binding 只表示借用。
2. 明确 `request`、`sink->headers`、`sink->body` 均为必需 callback；不需要消费数据的
   调用方使用 no-op callback。
3. 明确 request body/headers、sink headers/body view 的借用期限均为 `request()` 调用期。
4. 明确最大响应 header/body 字节数由 Adapter 在 sink 前强制执行。
5. 记录 source-level API 兼容目标，不承诺不同平台的二进制 ABI。

`agent_transport_t`、HTTP request/sink 仍是 Provider 扩展 API，不应被 `agent.h` 自动
聚合给仅使用本地 Model 的应用。

## 不采用的方案

### 在 Runtime 中注入 http_post

会把 HTTP 强加给所有 Agent，且无法表达 streaming body、headers、deadline、cancel、
连接池和 TLS 所有权；拒绝。

### Core 直接调用平台 HTTP SDK

使 Core 依赖 ESP-IDF、openvela、RT-Thread 或 Host 库，无法由用户裁剪；拒绝。

### Provider 直接调用平台 SDK

单平台原型可采用，但不是跨平台 Model Provider 的主路径；仅允许位于平台专用 addon。

### 首版公开 start/poll/cancel

需要请求句柄、事件线程、背压、buffer 生命周期、取消后的回调排空和 Core 状态机；在
同步 Transport 及其内存测量验证前暂缓。

### 自动重试和重定向

可能重放已发送的非幂等请求；由产品/Provider 显式决策。

## 验收要求

- 同一套 Transport 契约分别由 Mock、Host 和至少一个 MCU Adapter 编译验证。
- 完整 response、零长度 body、分块 body、超大 header/body、畸形 chunk、TLS/DNS/I/O
  失败、HTTP 401/429/5xx、cancel 和 deadline 均有测试。
- 断言 headers 先于 body、sink 错误后无更多 callback、`request` 返回后无 sink callback。
- 异步平台 Adapter 验证跨任务 callback 不接触 Agent turn；取消/超时后排空再返回。
- 报告 Adapter state、HTTP buffer、TLS heap、网络任务栈和 cJSON DOM 的峰值；这些不属于
  Core `agent_workspace_t`，但必须进入产品内存预算。
- 最小构建不链接任何 HTTP/TLS 库；未选中的 Adapter 不进入 ROM 或依赖图。

## 影响

该方案保持 Core 的网络无关性，允许用户按平台和产品裁剪依赖，并保留应用自定义
Transport 的能力。代价是每个平台 Adapter 都要实现同步语义、取消排空和有界内存。
未来若真实产品需要非阻塞多请求，需另立 ADR，在不破坏此同步 Profile 的前提下扩展。
