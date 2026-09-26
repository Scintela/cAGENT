# ADR 0008: 统一错误码与模块边界

- 状态：已接受
- 日期：2026-09-24

## 背景

cAgentV2 的 Core、Model Provider、Transport、Context、Tool 和 Session 需要在 ESP-IDF、
openvela、RT-Thread 与 Host 上用同一方式报告失败。平台的 errno、mbedTLS、HTTP、JSON
和设备 SDK 各自有错误体系；将它们穿透到公共 API 会使应用代码依赖某个 Port 或 Provider。

此前草案尝试将错误码、错误来源和平台诊断组合为公共错误对象。这个设计对完整诊断有用，
但为每个结果、Event 和 callback 传递额外字段，且错误来源常可由函数边界和 Event kind
直接得知。它不符合首版小型 MCU Core 的接口目标。

本 ADR 参考 NuttX 与 Linux 内部接口的共同约定：成功为零，失败为负值；底层函数直接
返回规范错误码，调用方在其自己的模块边界记录上下文。NuttX 内部接口使用负 errno 而
非隐式修改 `errno`；Linux 内核广泛使用零成功、负错误码失败。cAgent 采用这个返回模型，
但不暴露任一 OS 的 errno 编号。

## 决定

### 只公开一种错误返回类型

公共错误返回类型为扁平 enum：

```c
typedef enum {
    AGENT_OK = 0,
    AGENT_ERROR = -1,
    /* ... */
} agent_error_t;
```

所有以状态表示成功或失败的 Core API、Provider、Transport、Context、Tool 和 Storage
内部边界都返回 `agent_error_t`。创建型 API 可继续以 NULL 表示失败，并提供配套的
状态返回型初始化入口；销毁型 API 可为 void。只有 `AGENT_OK` 表示成功；所有错误值为
负数，正值不是合法的成功或控制流结果。enum 的底层大小不作为跨编译器二进制 ABI 承诺，
库仅承诺同版本源码构建的 API 语义。首版不公开：

```c
struct agent_error {
    agent_error_t code;
    agent_error_source_t source;
    int native_code;
};
```

也不保存 Agent 级 `last_error`。错误来源由调用位置、模块日志和 Event kind 表达：

```c
ret = model_complete(model, request, sink);
if (ret != AGENT_OK) {
    agent_log_model_failure(agent, ret);
    return ret;
}
```

此处 `model_complete()`、`AGENT_EVENT_MODEL_END` 和日志前缀已经说明失败发生在 Model
边界；无需把同一事实再复制进 Error ABI。终态 `agent_response_t.status` 与
`agent_step_result_t.status` 只保存错误码。

### 错误码集合

错误常量使用 `AGENT_ERROR_*` 前缀；数值自本 ADR 接受起冻结，新增项只能追加，不得
重排或复用已发布数值。首版集合如下：

```c
typedef enum {
    AGENT_OK = 0,

    /* Generic errors. */
    AGENT_ERROR = -1,                  /* External failure without a better category. */
    AGENT_ERROR_NOMEM = -2,            /* Allocator or provider heap allocation failed. */
    AGENT_ERROR_INVALID = -3,          /* Invalid argument, configuration, or local data. */
    AGENT_ERROR_STATE = -4,            /* Lifecycle state does not permit the operation. */
    AGENT_ERROR_BUSY = -5,             /* Active turn prevents an idle-only operation. */
    AGENT_ERROR_LIMIT = -6,            /* Configured execution or bounded-storage limit. */
    AGENT_ERROR_TIMEOUT = -7,          /* Effective deadline elapsed. */
    AGENT_ERROR_CANCELLED = -8,        /* Cooperative cancellation observed. */
    AGENT_ERROR_NOT_FOUND = -9,        /* Named object is absent. */
    AGENT_ERROR_EXISTS = -10,          /* Named object already exists. */
    AGENT_ERROR_NOT_SUPPORTED = -11,   /* Feature is unavailable in this Profile/build. */
    AGENT_ERROR_IO = -12,              /* I/O did not complete successfully. */
    AGENT_ERROR_AUTH = -13,            /* Remote credential or authorization rejected. */
    AGENT_ERROR_TRUNCATED = -14,       /* Final output was not fully delivered. */
    AGENT_ERROR_PARSE = -15,           /* Non-provider input or wire format cannot be parsed. */

    /* Module errors. */
    AGENT_ERROR_CONTEXT_OVERFLOW = -32,/* Context projection exceeded its byte limit. */
    AGENT_ERROR_MODEL_FAILED = -48,    /* Model failed without a more precise code. */
    AGENT_ERROR_MODEL_PARSE = -49,     /* Model response cannot be parsed. */
    AGENT_ERROR_MODEL_RATE_LIMIT = -50,/* Model service rejected the request due to a quota. */
    AGENT_ERROR_MODEL_UNAVAILABLE = -51,/* Model service is temporarily unavailable. */
    AGENT_ERROR_POLICY_DENIED = -64,   /* Policy or confirmation rejected execution. */
    AGENT_ERROR_TOOL_ARGUMENT = -80,   /* Model-supplied Tool arguments are invalid. */
    AGENT_ERROR_TOOL_FAILED = -81      /* Tool failed without a more precise code. */
} agent_error_t;
```

| 错误码 | 含义与使用边界 |
|--------|----------------|
| `AGENT_OK` | 操作成功；唯一成功状态。 |
| `AGENT_ERROR` | 无法进一步归类的外部或遗留 callback 失败；Core 已知具体原因时不得使用。 |
| `AGENT_ERROR_NOMEM` | allocator、workspace 或 Provider 所需内存无法取得。 |
| `AGENT_ERROR_INVALID` | 调用参数、配置值或本地输入数据不合法。 |
| `AGENT_ERROR_STATE` | 当前生命周期状态不允许该操作，例如未完成初始化或已停止。 |
| `AGENT_ERROR_BUSY` | 活动 turn 或其他互斥操作暂时阻止空闲期操作；等待当前操作结束后可重试。 |
| `AGENT_ERROR_LIMIT` | 配置的 steps、Tool 调用、消息、scratch、Session pool 或一般字节预算达到上限。 |
| `AGENT_ERROR_TIMEOUT` | 有效 deadline 已到；不能据此推断外部 Tool 没有产生副作用。 |
| `AGENT_ERROR_CANCELLED` | 在协作式取消检查点观察到外部取消请求。 |
| `AGENT_ERROR_NOT_FOUND` | 指定名称或 ID 的对象不存在。 |
| `AGENT_ERROR_EXISTS` | 注册、创建或绑定的名称或 ID 已存在。 |
| `AGENT_ERROR_NOT_SUPPORTED` | 当前 Profile、构建配置或明确协商的远端能力不支持该功能。 |
| `AGENT_ERROR_IO` | DNS、socket、TLS、串口、设备 I/O 或链路不可用导致操作未完成。 |
| `AGENT_ERROR_AUTH` | 远端凭证或授权被拒绝，例如 HTTP `401`、`403`。 |
| `AGENT_ERROR_TRUNCATED` | 最终输出未能完整交付到应用 buffer；它是交付状态，不应触发 Model 或 Tool 重执行。 |
| `AGENT_ERROR_PARSE` | 非模型专属的配置、HTTP envelope、协议帧或其他输入格式无法解析。 |
| `AGENT_ERROR_CONTEXT_OVERFLOW` | 完整 Context 投影超过其专属字节预算；不用于请求序列化或一般 buffer 不足。 |
| `AGENT_ERROR_MODEL_FAILED` | Model Provider 失败但没有更准确的 Model 语义，例如被远端拒绝的请求。 |
| `AGENT_ERROR_MODEL_PARSE` | 已成功取得模型响应，但其 payload 不符合 Provider 的响应契约。 |
| `AGENT_ERROR_MODEL_RATE_LIMIT` | Model 服务因配额或限流拒绝请求，例如 HTTP `429`；可由产品按退避策略决定是否重试。 |
| `AGENT_ERROR_MODEL_UNAVAILABLE` | Model 服务暂时不可用，例如 HTTP `5xx`；Core 不自动重试。 |
| `AGENT_ERROR_POLICY_DENIED` | 本地 Agent policy 或确认流程拒绝执行；不表示远端服务拒绝。 |
| `AGENT_ERROR_TOOL_ARGUMENT` | Model 提供的 Tool arguments 不符合该 Tool 的参数契约。 |
| `AGENT_ERROR_TOOL_FAILED` | Tool 已进入执行边界但失败，且没有更准确的通用或模块错误码。 |

`AGENT_ERROR` 是第三方 callback、旧 addon 或无法进一步分类的外部失败的兜底值；Core
已知失败原因时不得用它替代具体错误码。`MODEL_FAILED` 和 `TOOL_FAILED` 也只作为各自
模块无法提供更具体语义时的领域兜底。`PARSE` 用于通用配置、HTTP envelope、协议帧或
其他非模型专属格式解析；成功收到模型响应但其内容不符合 Model 契约时使用
`MODEL_PARSE`；模型给出的 Tool arguments 不合法时使用 `TOOL_ARGUMENT`。

`LIMIT` 同时覆盖配置的 steps、Tool 调用、消息、scratch、Session pool 和一般字节上限。
调用方不得仅凭 `LIMIT` 或 `CONTEXT_OVERFLOW` 决定淘汰 Session；仅 Context/Session 投影
阶段可在本模块确认裁剪历史后重试。`CONTEXT_OVERFLOW` 仅表示完整 Context 投影超过其字节
预算，不能用于 JSON 转义、Model HTTP request 序列化、Tool schema 或应用输出 buffer 不足。
这些情况使用 `LIMIT`。最终 assistant 输出写入应用 buffer 时允许部分交付则使用
`TRUNCATED`；它是交付结果而不是 Model 或 Tool 的再次执行理由。

`STATE` 与 `BUSY` 不合并：前者表示对象尚未 start、turn 已结束等生命周期不合法；后者
表示活动 turn 暂时阻止本应在空闲期可用的重配置、注册或清理操作。

### 平台与 Provider 错误归一化

Core 永远不返回原始 errno、HTTP status、mbedTLS、cJSON 或设备 SDK 值。Port、Adapter
和 Provider 在进入 Core 前完成一次映射：

```text
errno / socket / mbedTLS / serial / device SDK
                 -> AGENT_ERROR_IO | TIMEOUT | CANCELLED | NOMEM

HTTP 401 / 403  -> AGENT_ERROR_AUTH
HTTP 408 / 504  -> AGENT_ERROR_TIMEOUT
HTTP 429        -> AGENT_ERROR_MODEL_RATE_LIMIT
HTTP 5xx        -> AGENT_ERROR_MODEL_UNAVAILABLE
HTTP 400 / 404 / 422 or other rejected model request
                 -> AGENT_ERROR_MODEL_FAILED
malformed HTTP envelope / generic protocol frame
                 -> AGENT_ERROR_PARSE
successful model response with malformed or incompatible payload
                 -> AGENT_ERROR_MODEL_PARSE
unknown vendor failure
                 -> AGENT_ERROR_MODEL_FAILED
```

Transport 的同步、取消与 deadline 行为仍遵从 [ADR 0007](0007-http-transport-adapters.md)。
Transport 完整交付 HTTP status/body；OpenAI-compatible Model Provider 负责将远端响应
映射为 `AUTH`、`MODEL_FAILED`、`MODEL_PARSE` 或其他适当的 Agent code。Core 不自动重试、
重定向或重放模型请求和副作用 Tool。

Provider/Adapter 可以记录私有 native code、HTTP status、远端 body 摘要或 TLS 原因，但
这些诊断不得进入通用 public struct、函数返回值或 Session payload。它们只能通过模块
专用日志、诊断 callback 或产品 telemetry 获取，并且不得记录 API key、Authorization
header、完整用户输入或隐私数据。

远端 Tool/MCP/WebSocket addon 保持自己的协议错误集合，不将 `OFFLINE`、`VERSION` 或
`PROTOCOL` 加入 Core enum。适配器应在实际调用边界映射：远端凭证拒绝为 `AUTH`，明确的
版本/能力不支持为 `NOT_SUPPORTED`，远端业务失败为 `TOOL_FAILED`，链路不可用为 `IO`。
`POLICY_DENIED` 只表示本地 Agent policy 或确认流程拒绝，不能泛化为远端服务拒绝。

### 传播与观测

错误码沿调用链原样传播，除非当前模块能够提供更准确的 Agent 语义。调用者必须检查负
返回值；Core 内部可使用仅限实现目录的 `AGENT_RETURN_ON_ERROR(expr)` 辅助宏，宏必须只
求值参数一次。该宏不作为首版公共 API。

| 边界 | 约定 |
|------|------|
| `agent_plan()`、`agent_init()`、注册/注销、空闲期设置 | 直接返回 code；没有活动 turn 或结果对象。 |
| `agent_turn_begin()`、`agent_turn_resume()`、`agent_cancel()` | 直接返回请求是否被接受的 code。 |
| `agent_turn_step()` | 参数、句柄和调用状态错误时直接返回负 code；有效 step 即使结束为失败也返回 `AGENT_OK`，终态 code 写入 `agent_step_result_t.status`。 |
| `agent_run()` | 返回最终执行 code，并在有效 `agent_response_t` 中写入相同 `status`。 |
| `agent_event_t` | `status` 为该 Event 边界的 code；`kind` 是来源上下文，例如 MODEL_END、TOOL_END 或 TURN_END。 |

`agent_response_t.delivery_status` 保持独立：应用输出 buffer 不足时为
`AGENT_ERROR_TRUNCATED`，但已完成的模型和工具不重新执行。`agent_response_t.status`
仍表示执行链的最终 code。

在同一检查点 deadline 与 cancel 同时成立时，Core 先检查 deadline，再检查 cancel；
因此 `TIMEOUT` 优先。这个规则只固定最终 code，不声称外部动作没有发生。

### Tool 副作用不属于 Error ABI

Tool handler 是否已经影响设备不是“错误来源”。即使首版只使用单一 error code，Tool
仍应在其执行结果或 Session 事实中记录独立的 effect：

```c
typedef enum {
    AGENT_TOOL_EFFECT_NOT_STARTED = 0,
    AGENT_TOOL_EFFECT_COMPLETED,
    AGENT_TOOL_EFFECT_UNKNOWN
} agent_tool_effect_t;
```

例如 `AGENT_ERROR_TIMEOUT` 不能说明命令是否已发出。Tool 未执行时由 Core 记录
`NOT_STARTED`；一旦开始调用 handler，默认应为 `UNKNOWN`，仅 Tool 能确认动作完成后才
报告 `COMPLETED`。该结构和具体 callback 签名由 `tool.h` 后续收敛，不把 effect 塞进
`agent_error_t` 或公共错误对象。

`POLICY_DENIED` 与 `TOOL_ARGUMENT` 通常是某一次 Tool 调用结果，而不一定是整个 turn
的终态失败。默认 ReAct Loop 可以将它们作为 Tool result 回送 Model，让模型改用其他工具
或给出 final；`agent_run()` 因而仍可能最终返回 `AGENT_OK`。

### 字符串诊断

调用方直接使用 `code != AGENT_OK` 判断失败，不提供 `agent_error_failed()` 或
`agent_error_succeeded()` 包装。`agent_error_str(agent_error_t code)` 返回稳定 ASCII 标识，未知值返回
`"AGENT_ERROR_UNKNOWN"`，且永不返回 NULL。字符串仅用于日志和诊断显示，不作为机器间
协议、持久化格式或控制流条件。

## 不采用的方案

### `code + source + native_code` 公共错误对象

调用边界与 Event kind 已经表达大多数来源；native code 又与 Port/Provider 耦合。将它们
放入每个结果和 callback 会扩大公共 ABI，不能改善低层 Provider 的实际诊断能力；拒绝。

### 按模块与错误种类的笛卡尔积扩张

不增加 `MODEL_TIMEOUT`、`TOOL_TIMEOUT`、`HTTP_TIMEOUT`、`TOOL_NETWORK_ERROR`、
`TOOL_NOT_FOUND`、`TOOL_DISABLED`、`OFFLINE`、`PROTOCOL` 或 `VERSION` 等。它们会使枚举
随模块和平台组合增长；统一使用 `TIMEOUT`、`IO`、`NOT_FOUND`、`NOT_SUPPORTED` 等通用
code，并用函数边界、Event、Tool result 与模块日志表达上下文。

### 直接公开 errno、HTTP 或 SDK 错误值

这些值不具备跨 Port 的稳定语义，可能与 Agent 负值冲突，也会让应用依赖某个具体网络或
设备实现；拒绝。

### Agent 级 `last_error` 或线程局部 errno 模拟

错误会被后续调用覆盖，且不能对应特定 turn、Event 或 Tool 调用。调用者应使用直接返回
值、结果 status 与同步 Event；拒绝。

### Linux `ERR_PTR()`

`agent_create()` 继续以 NULL 表示失败，精确 code 使用 `agent_init()` 获得。错误指针会
使 MCU 应用更容易误解引用，也不适合 caller workspace 模式；拒绝。

### Core 自动重试

I/O、超时和取消可能发生在设备或远端请求已被接受之后。重试由了解幂等键、设备 ACK 和
远端协议的 Tool、Provider 或产品决定；拒绝。

## 影响

正面影响：

- 所有常见失败路径仅传递一个扁平 enum，适合 MCU callback、同步 I/O 和无堆 Profile；
- API 与 Provider 采用统一的零成功、负错误返回约定；
- 代码、日志和测试可通过明确的函数边界识别错误来源，无需 Error 对象或 error stack；
- 平台错误被隔离在 Adapter/Provider 内，Core 维持源码级可移植性；
- Tool 副作用事实仍独立保留，不会因精简错误接口而诱导不安全重试。

代价：

- 最终 `agent_run()` 的单个 code 不携带完整来源；产品需要读取 Event/日志或 Provider
  专用诊断才能定位深层失败；
- Provider 和 Tool 作者必须在模块边界正确映射错误，不能把所有失败退化为
  `AGENT_ERROR`；
- `error.h`、结果结构、callback 和内部协作声明使用 `agent_error_t`；后续新增能力
  必须遵守通用/模块两组边界，不能重新引入来源对象。

`src/core/error.c` 实现 `agent_error_str()` 的完整稳定映射。后续新增 code 时必须同时更新
该映射和对应的验证用例。

## 验证要求

- 每个 public API 在参数、状态、并发、资源与上限错误下返回约定 code，失败不留下半注册
  项或活动 turn；
- 所有 Model、Transport、Context 与 Tool callback 都在 Host 和至少一个 MCU Profile 下
  验证零成功、负错误传播和 unknown vendor failure 归一化；
- Mock Transport 覆盖 DNS/TLS/I/O、deadline、cancel、401/403、408/504、429、5xx、
  被拒绝的模型请求、畸形 HTTP envelope 与畸形模型 JSON，验证 `IO`、`TIMEOUT`、
  `CANCELLED`、`AUTH`、`PARSE`、`MODEL_FAILED`、`MODEL_PARSE`、`MODEL_RATE_LIMIT` 与
  `MODEL_UNAVAILABLE` 的边界；
- `turn_step()` 的 API 调用成功与执行终态失败分别验证；`agent_run()` 返回值必须与
  `response->status` 一致；
- 输出 buffer 不足只影响 `delivery_status`，不会再次调用 Model 或 Tool；
- Tool 覆盖校验拒绝、Policy 拒绝、动作前失败、动作成功、发出命令后超时，验证 effect 与
  Session 的部分执行事实；
- deadline/cancel 同时成立时稳定返回 `TIMEOUT`；
- `agent_error_str()` 对每个定义 code 和未知 code 返回稳定、非 NULL 的 ASCII 字符串；
- Host 下以 AddressSanitizer、UndefinedBehaviorSanitizer 和 LeakSanitizer 覆盖错误、取消
  与清理路径。
