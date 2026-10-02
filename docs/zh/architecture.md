# cAgentV2 总体架构设计

## 1. 文档状态

本文定义 cAgentV2 的目标架构和首阶段实现边界，用于指导公共头文件、内部模块、
测试和迁移工作。本文中的 C 接口是架构草案，不代表已经实现或形成稳定 ABI。
其中 `agent_turn_begin/step/resume/end` 和通用 Plugin scope 属于后续候选，
不是同步 MVP 公开接口；当前边界以 [ADR 0020](adr/0020-synchronous-run-mvp.md)
和实际头文件为准。

## 2. 背景

当前 cAGENT V1 已经具备同步 ReAct loop、Model provider、Tool、Skill、Context、
Session、Runtime、Policy 和 Event 等基本能力，证明了轻量 C Agent 的主要执行路径
可行。

V1 同时暴露出以下需要在新架构中系统解决的问题：

- Agent 内部嵌入多个按编译期最大值分配的数组，常驻内存较大。
- 运行期逻辑容量不能减少 `agent_t` 的实际内存占用。
- 通用 Model 请求携带 OpenAI-compatible JSON，Core 与厂商协议边界不清晰。
- Model、Runtime、Tool 和 Skill 的内存所有权与销毁方式不统一。
- request limits、deadline、arena 和 Tool 调用计数临时写入 Agent 全局状态。
- 同步 `agent_run()` 难以自然支持用户确认、异步 I/O、暂停和恢复。
- Session 主要保存模型消息，不是可恢复、可回放的完整运行事实流。
- 扩展点数量较多，但缺少统一的挂载、失败回滚和卸载作用域。
- 部分公共字段和标志已经声明，却没有形成完整行为契约。

cAgentV2 不以“整理 V1 文件”为目标，而是保留 V1 已验证的行为和领域规则，重新
建立资源、生命周期、扩展和协议边界。

## 3. 定位

cAgentV2 的定位是：

> 一个静态可组合、资源可预算、类型安全、运行可追踪的嵌入式 Agent 微内核。

核心属性：

- **静态可组合**：扩展主要通过静态链接和启动期挂载，不依赖动态库加载。
- **资源可预算**：初始化前能够计算 Core、Session、运行 scratch 和插件状态的
  内存需求。
- **类型安全**：Model、Transport、Storage 等能力使用独立的强类型 ops。
- **运行可追踪**：模型可见的关键事实能够从有界 Session 事件日志重建。
- **平台可移植**：核心不直接依赖 POSIX、openvela、ESP-IDF 或 STM32 API。
- **安全默认**：未安装确认或 Policy 能力时，高风险 Tool 默认拒绝。
- **渐进能力**：简单应用继续使用同步接口，复杂应用可以驱动状态机。

本项目的移植目标是让同一应用源代码在 ESP-IDF、openvela、RT-Thread 和 Host port
上使用同一套公共 API、资源模型和运行语义。该承诺是源码级和行为级兼容，不承诺
跨平台二进制 ABI。产品只替换 Profile 中的 Runtime port 与 Provider；具体硬件服务
通过应用自己的 Tool `user_data` 或设备适配层接入，不进入 Core。

## 4. 非目标

V2 第一阶段不实现：

- 动态 `.so`、`.dll` 或运行期符号解析；
- 从网络下载和安装二进制插件；
- Agent 运行期间任意热更新插件；
- 基于字符串的通用 Service Locator；
- 所有模块都通过 Event Bus 间接通信；
- 复杂的插件依赖求解器；
- 多 Agent 调度、子 Agent 和分布式编排；
- 完整 RAG、向量数据库或长期 Memory 产品方案；
- UI、语音、MQTT、WebSocket 和具体设备业务逻辑；
- 无界 Session 或无界 trajectory 日志。

这些非目标用于防止 V2 在基础契约尚未稳定时演变成重量级应用框架。

## 5. 设计原则

### 5.1 小内核

Kernel 只负责无法安全外移的能力：

- Agent 生命周期状态；
- workspace 和资源预算；
- 单次运行状态机；
- 类型化能力注册表；
- Session turn 和 Tool call/result 完整性；
- cancel、deadline、错误与统计；
- live event 派发；

Model 协议、HTTP、文件系统、业务 Tool 和 Skill loader 不进入 Kernel。

### 5.2 类型化能力

不同能力使用不同接口：

```c
agent_model_ops_t
agent_transport_ops_t
agent_session_storage_ops_t
agent_memory_ops_t
agent_loop_ops_t
```

禁止把所有能力退化为：

```c
void *agent_get_service(agent_t *agent, const char *name);
```

字符串查找和 `void *` 会丢失编译期检查，使拼写、签名和生命周期错误推迟到运行期。

### 5.3 显式所有权

每个指针必须属于以下语义之一：

```text
BORROWED    调用者持有，使用期间必须保持有效
COPIED      cAgent 复制到自己的 workspace
TRANSFERRED 所有权转移给 cAgent，销毁时由 cAgent 释放
VIEW        临时只读视图，只在当前调用或回调期间有效
```

不得只通过函数名或注释猜测所有权。

### 5.4 有界资源

Tool、Skill、Context provider、Session、事件、消息 payload、Tool 参数、Tool 输出、
模型输出和 scratch buffer 都必须有明确上限。

模块是否存在与所有 Core 容量均由 build Profile 的编译期配置决定。Profile 可以裁剪未用
模块、Core registry/scratch 容量与 Session Storage Provider Profile；运行期不得扩容、heap fallback 或
创建每轮临时 arena。

### 5.5 失败可回滚

创建、插件挂载、批量注册和 Session 写入必须具有明确事务边界。中途失败时，已经
完成的操作按相反顺序撤销，不能留下半注册 Tool、悬空 callback 或未完成插件状态。

### 5.6 主链路直接调用

Core 调用 Model、Session 和 Tool 时使用直接的类型化函数调用。Event 用于观测、
持久化事实和明确的拦截点，不替代所有控制流。

不引入统一的 "Core Gateway" 门面对象：Loop 不持有 Tool/Skill Registry 指针，
只接收 Kernel 投影后的只读视图（如 `agent_model_request_t` 中的 Tool 列表）。
模块间依赖方向由头文件包含关系在编译期保证，而不是由运行时网关转发。

## 6. 核心概念

V2 区分以下概念：

| 概念 | 定义 | 示例 |
|------|------|------|
| Capability | 一类能力的强类型接口 | Model、Transport、Session Storage |
| Provider | Capability 的具体实现 | OpenAI Model、openvela HTTPS |
| Contribution | 向某个注册表贡献的对象 | 一个 Tool、Skill、Context provider |
| Plugin | 打包并管理一组 Provider/Contribution 的生命周期单元 | Smart Home Plugin |
| Profile | 一组 Plugin 和核心配置的组合 | Tiny Chat、Device ReAct |

一个 Smart Home Plugin 可以同时贡献：

```text
Smart Home Plugin
├── Tool: read_sensor
├── Tool: set_power
├── Tool: reboot
├── Context: device_state
├── Policy: dangerous_action_confirmation
└── Observer: tool_audit
```

Plugin 不是动态共享库的同义词。在 V2 中，它首先表示静态链接组件的生命周期和
作用域。

## 7. 总体分层

```text
Application / Product
│
├── Profile / Composition
│     选择 Model、Transport、Tool Pack、Storage、Policy
│
├── Typed Plugins
│     ├── Model Provider
│     ├── Transport Provider
│     ├── Tool Pack
│     ├── Skill Pack
│     ├── Context Provider
│     ├── Session Storage
│     ├── Memory Provider
│     └── Policy / Observer
│
└── cAgentV2 Kernel
      ├── Lifecycle
      ├── Workspace / Resource Planner
      ├── Capability Registry
      ├── Run State Machine
      ├── Session Event Log
      ├── Context Projection
      ├── Tool Execution Pipeline
      ├── Event Dispatch
      └── Cancel / Deadline / Stats
```

依赖方向必须保持单向：

```text
Application -> Plugin -> Public Capability API -> Kernel
Provider -> Transport -> Runtime/Port
Kernel -X-> 厂商协议、文件系统、具体平台 API
```

### 7.1 模块分类导读

全部模块按以下五类归属，依赖只允许从上层指向下层：

| 分类 | 模块 | 说明 |
|------|------|------|
| Core Kernel | Lifecycle、Workspace、Run State Machine、Registry、Event Dispatch、Cancel/Deadline/Stats | 框架机制，不含智能逻辑 |
| Execution / Orchestration | Loop（默认 ReAct）、Context Projection、Tool Pipeline | 运行控制流，Kernel 拥有状态机 |
| Capabilities | Model、Tool、Skill、Session、Memory | 类型化 Capability 与 Contribution |
| Cross-cutting Control | Policy Chain、Confirmation、Validation、Limits | Tool Pipeline 的固定阶段，不是独立服务 |
| Platform Services | Runtime、Transport、Session Storage | 平台与持久化抽象 |

外围系统（Trigger、Scheduler、UI、Voice、MQTT、Sensor）不属于 cAgentV2 Core，
由 Application 直接驱动运行接口。各模块的职责与契约见 `docs/arch/` 下对应的
模块文档。

## 8. Kernel 内部结构

建议的内部对象：

```c
struct agent {
    agent_identity_t identity;
    agent_runtime_t runtime;
    agent_workspace_t workspace;

    agent_lifecycle_t lifecycle;
    agent_run_control_t run_control;

    agent_model_binding_t model;

    agent_tool_registry_t tools;
    agent_skill_registry_t skills;
    agent_context_registry_t contexts;
    agent_policy_chain_t policies;

    agent_session_manager_t sessions;
    agent_memory_manager_t memory;

    agent_event_dispatcher_t events;
    agent_stats_t stats;
};
```

Transport 不是 Agent 的全局成员。需要网络的 Model Provider 在其自身配置中借用平台
Adapter；本地 Model、Mock 和串口 Model 不产生 Transport 依赖。具体边界见
[ADR 0007](adr/0007-http-transport-adapters.md)。

这是“Agent 统一拥有生命周期、模块独立维护状态”，而不是把所有数组、计数和缓存
平铺在一个巨型结构体中。

模块归属示例：

| 状态 | 所属模块 |
|------|----------|
| Tool entries、schema cache、schema generation | Tool Registry |
| Session binding/cursor 与完整 turn 不变量 | Core；历史记录/缓存由 Session Storage Provider |
| Model 指针、ops、ownership | Model Binding |
| deadline、effective limits、step count | Run Context |
| plugin mount records、cleanup stack | Plugin Manager |

## 9. 生命周期

### 9.1 Agent 状态

```c
typedef enum {
    AGENT_LIFECYCLE_UNINITIALIZED = 0,
    AGENT_LIFECYCLE_CONFIGURING,
    AGENT_LIFECYCLE_READY,
    AGENT_LIFECYCLE_RUNNING,
    AGENT_LIFECYCLE_STOPPED,
    AGENT_LIFECYCLE_DESTROYED
} agent_lifecycle_state_t;
```

允许的主要转换：

```text
UNINITIALIZED
    -> CONFIGURING
        -> READY
            -> RUNNING
                -> READY
            -> STOPPED
                -> CONFIGURING（可选）
                    -> READY
    -> DESTROYED
```

第一阶段建议采用严格规则：

- 通用 Plugin mount/unmount 延后；静态能力包通过普通装配函数直接注册自己的贡献并自行回滚。
- Tool/Context/Skill 注册、注销和启停只允许在 CONFIGURING/READY。
- ACTIVE 状态禁止修改注册表和替换 Provider。
- 同一个 Agent 只允许一个活动 run/turn。
- `agent_destroy()` 不允许与运行并发。

### 9.2 初始化和清理

```text
validate build Profile and runtime config
-> bind fixed workspace
-> initialize kernel fixed pools
-> application registers/binds optional capabilities
-> validate Core ready state without I/O
-> READY
```

任意步骤失败时，撤销已建立的 Core 引用和装配项；caller workspace 仍由调用方持有。

## 10. 运行状态机

### 10.1 单次运行状态

```c
typedef enum {
    AGENT_RUN_IDLE = 0,
    AGENT_RUN_BUILDING_CONTEXT,
    AGENT_RUN_WAITING_MODEL,
    AGENT_RUN_WAITING_TOOL,
    AGENT_RUN_WAITING_CONFIRMATION,
    AGENT_RUN_COMPLETED,
    AGENT_RUN_FAILED,
    AGENT_RUN_CANCELLED
} agent_run_state_t;
```

单次运行使用独立上下文：

```c
typedef struct {
    const agent_request_t *request;
    agent_session_t *session;

    agent_limits_t limits;
    agent_cancel_token_t cancel;

    uint64_t start_ms;
    uint64_t deadline_ms;

    uint32_t step_count;
    uint32_t tool_call_count;

    agent_arena_t scratch;
    agent_run_state_t state;
} agent_run_context_t;
```

request override 不再临时覆盖 Agent 默认配置。Policy、Event、Tool 和 Model 都通过
run context 获得当前 session、trace、deadline 和有效 limits。

### 10.2 两层运行接口

复杂应用使用可驱动接口：

```c
agent_error_t agent_turn_begin(agent_t *agent,
                                    const agent_request_t *request,
                                    agent_turn_t **turn);

agent_error_t agent_turn_step(agent_turn_t *turn,
                                   agent_step_result_t *result);

agent_error_t agent_turn_resume(agent_turn_t *turn,
                                     const agent_resume_t *resume);

void agent_turn_end(agent_turn_t *turn);
```

简单应用继续使用同步接口：

```c
agent_error_t agent_run(agent_t *agent,
                             const agent_request_t *request,
                             agent_response_t *response);
```

同步接口是默认状态机的驱动封装，不维护另一套 ReAct 实现。

### 10.3 用户确认

高风险 Tool 不应把“需要确认”退化成普通错误。状态机进入：

```text
WAITING_CONFIRMATION
```

上层通过 `agent_turn_resume()` 提交允许或拒绝决定。未安装确认能力时，带有
`REQUIRES_CONFIRM` 标志的 Tool 默认拒绝。

### 10.4 Timeout 和取消

必须区分：

- Core checkpoint deadline；
- Transport I/O timeout；
- Provider cancel；
- Tool cooperative cancel；
- ISR-safe cancel request。

建议提供：

```c
agent_error_t agent_cancel(agent_t *agent);
agent_error_t agent_cancel_from_isr(agent_t *agent);
```

ISR 版本只更新 Runtime 提供的原子/临界区标志，不调用网络、锁、日志和 Provider
callback。

## 11. Session 事件模型

> 本节中的事件种类仍描述 Core 必须维护的完整 turn/Tool 配对不变量；关于完整历史的所有权、
> Storage Provider、投影窗口和内存域，以 ADR 0016 为准。本文早期的 Core event/payload pool
> 与直接 storage mount 签名均为已被替代的架构草图。

### 11.1 Session 是运行事实，不是厂商消息 JSON

V2 Session 使用有界 append-only 事件日志：

```c
typedef enum {
    AGENT_SESSION_EVENT_TURN_BEGIN = 1,
    AGENT_SESSION_EVENT_USER_MESSAGE,
    AGENT_SESSION_EVENT_CONTEXT_INJECTED,
    AGENT_SESSION_EVENT_ASSISTANT_MESSAGE,
    AGENT_SESSION_EVENT_ASSISTANT_TOOL_CALL,
    AGENT_SESSION_EVENT_TOOL_RESULT,
    AGENT_SESSION_EVENT_TURN_END,
    AGENT_SESSION_EVENT_TURN_ABORT
} agent_session_event_type_t;
```

数据流：

```text
Session Event Log
    -> Session Projection
    -> Canonical Model Messages
    -> Model Provider Serialization
    -> Vendor Protocol
```

Kernel 负责以下不变量：

- user message 开启 turn；
- assistant Tool call 必须与 Tool result 对应；
- 同一个 call ID 不允许重复写入 result；
- 只有完整 turn 可以被淘汰或形成稳定 checkpoint；
- 失败和取消生成明确 abort 事实；
- Storage provider 不能绕过 Kernel 直接构造非法 Session 状态。

### 11.2 有界实现

```text
Session metadata slots
Event descriptor ring
Shared payload byte pool
Shared Tool-call descriptor pool
Optional persistent append backend
```

普通文本消息不再为最大数量 Tool calls 永久预留空间。

Context 追踪支持：

```text
NONE      不记录注入内容
METADATA  记录来源、版本、长度和 hash
FULL      保存完整模型可见内容
```

Tiny profile 默认使用 `METADATA`，资源充足的平台可以使用 `FULL`。

### 11.3 Session Storage

持久化通过类型化接口注入：

```c
typedef struct {
    int (*create)(void *provider, agent_string_view_t session_id);
    int (*open)(void *provider, agent_string_view_t session_id);
    int (*append)(void *provider,
                  const agent_session_record_view_t *record);
    int (*checkpoint)(void *provider,
                      const agent_session_checkpoint_view_t *checkpoint);
    int (*remove)(void *provider, agent_string_view_t session_id);
    int (*sync)(void *provider);
} agent_session_storage_ops_t;
```

JSONL、Flash journal 和 NVS 都是 Storage provider。Kernel 不直接调用 `fopen()`。

## 12. Model 接口

### 12.1 Provider-neutral 请求

Core 不构造 OpenAI-compatible messages/tools JSON。通用请求使用只读视图：

```c
typedef struct {
    agent_string_view_t system_prompt;

    const agent_message_view_t *messages;
    size_t message_count;

    const agent_tool_view_t *tools;
    size_t tool_count;

    agent_string_view_t session_id;
    agent_string_view_t trace_id;

    uint32_t timeout_ms;
    uint32_t max_output_tokens;
} agent_model_request_t;
```

不同 Provider 分别转换：

```text
OpenAI Provider     -> OpenAI JSON
Anthropic Provider  -> Anthropic JSON
Local Provider      -> local inference input
Mock Provider       -> deterministic test result
```

### 12.2 Response 所有权

Provider 不应返回生命周期含糊的内部指针。建议 Core 提供 response sink：

```c
typedef struct {
    int (*append_text)(void *context,
                       const char *data,
                       size_t size);

    int (*append_tool_call)(void *context,
                            const agent_tool_call_view_t *call);

    void *context;
} agent_model_response_sink_t;
```

Provider 将结果写入 Core 管理的 run scratch 或 Session transaction，避免下一次
Provider 调用导致旧 response 指针失效。

### 12.3 Model ops

```c
typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;

    int (*complete)(void *provider,
                    const agent_model_request_t *request,
                    agent_model_response_sink_t *sink,
                    const agent_cancel_token_t *cancel);

    int (*cancel)(void *provider);
    void (*destroy)(void *provider);
} agent_model_ops_t;
```

创建和销毁必须使用同一个 allocator 或 caller-provided state。

上面的 `complete()` 是同步最小接口。多平台网络 I/O 是否在第一版公开为
`start/poll/cancel` 非阻塞接口尚未决定：在此之前，Core 不创建线程，应用可在自己的
任务或主循环中驱动 `agent_turn_step()`。该决策必须与 `transport` 接口一起确定，避免
在 ESP-IDF、openvela、RT-Thread 上固化不兼容的阻塞模型。

## 13. Runtime 与 Transport

### 13.1 Runtime

Runtime 只包含基础平台能力：

```c
typedef struct {
    agent_allocator_t allocator;
    agent_clock_t clock;
    agent_logger_t logger;
    agent_sync_t sync;
    agent_critical_t critical;
} agent_runtime_t;
```

Runtime 在 Agent 初始化时绑定，生命周期内不可替换。回调必须成对验证：

```text
allocate/free
mutex create/destroy/lock/unlock
enter/exit critical
```

Runtime 是最小的平台 Port，不承担网络、TLS、文件系统或硬件服务适配。其职责只限于
allocator、单调时钟、同步/临界区和日志；缺少某项可选能力的 Port 必须通过 Profile
验证显式拒绝不兼容组合，不能静默降级。

### 13.2 Transport

HTTP、流式连接或自定义 modem 属于独立 Transport Capability，而非 Runtime callback。
当前候选边界见 [ADR 0007](adr/0007-http-transport-adapters.md)：首版以同步语义的单个
HTTP `request + headers/body sink` 交换为限，由构建期可裁剪的平台 Adapter 实现。

```text
Application-owned Transport Adapter
    -> borrowed by OpenAI Model Provider
        -> Model bound to Agent

Local Model Provider
    -> no transport required
```

Adapter 可以在不改变 Core 的情况下分别使用 ESP-IDF、openvela、RT-Thread 或 Host
网络实现。Core 不直接绑定或拥有 Transport；通用 binding 不包含 `cancel/destroy`，
取消使用请求的 token/deadline，具体 Transport 的清理由应用或 Provider 专用 API 管理。

ADR 0007 仍是提案；`include/agent/transport.h` 在实现任意 Adapter 前必须按其中的
借用、callback 和生命周期契约收敛，不能继续执行本节此前的旧 `cancel/destroy` 签名。

## 14. Tool、Policy、Skill 和 Context

### 14.1 Tool Pipeline

```text
lookup
-> visibility/state validation
-> argument size/schema validation
-> policy chain
-> confirmation
-> rate/resource guard
-> execute
-> output validation
-> Session event append
```

Tool 标志必须具有明确行为：

| 标志 | V2 语义 |
|------|---------|
| `HIDDEN` | 默认可见；置位后从模型可见 Tool 列表移除 |
| `READ_ONLY` | 无外部副作用，可供 Policy 优化决策 |
| `SIDE_EFFECT` | 进入副作用 Policy/审计链 |
| `REQUIRES_CONFIRM` | 没有明确确认时禁止执行 |
| `DISABLED` | 不可调用且不进入 schema |
| `PARALLEL_SAFE` | 只声明并行安全，不自动启用并行执行 |

V2 不提供 `agent_register_tool_simple()` 之类的便捷变体，调用者直接初始化
`agent_tool_t` 后注入。因此零初始化（`flags == 0`）必须表示“默认启用且对模型可见”，
由 `HIDDEN` / `DISABLED` 显式关闭；Skill 与 Context 同理，用 `*_FLAG_DISABLED`
反向控制，避免“注册成功但模型看不到”。

Tool 使用受限类型化 descriptor 注册，由内部 schema codec 生成并缓存模型可见 schema。
现有 `agent_tool_view_t.input_schema_json` 仅是模型投影视图，不是注册的规范来源。
Core 不依赖 JSON 或承诺完整 JSON Schema 支持；OpenAI 等 Provider 的协议 JSON、JSONL
storage 和 Tool 参数的 JSON 适配属于外围实现，并必须使用有界 buffer。第一版 JSON
依赖、动态分配边界和使用规则见 ADR 0006。

### 14.2 Policy Chain

V2 支持多个 Policy contributor：

```text
Permission Policy
-> Confirmation Policy
-> Rate-limit Policy
-> Product Policy
-> Tool handler
```

合并规则：

```text
任意 DENY             -> DENY
没有 DENY 但需要确认  -> REQUIRE_CONFIRM
全部允许              -> ALLOW
```

Policy request 必须包含 Tool、arguments、session、trace、run state 和调用来源。

### 14.3 Skill 与 Context

Skill 是上下文资源，不直接执行代码。Skill loader 可以作为插件，Core 只接收已经
解析和验证的 Skill contribution。

Context provider 必须明确优先级和溢出行为：

```text
CRITICAL  放不下则本次运行失败
NORMAL    根据预算策略裁剪、摘要或跳过
OPTIONAL  放不下直接跳过
```

## 15. 插件作用域（延后）

当前版本不实现通用 Plugin scope、descriptor 或 mount/unmount API。应用直接通过
类型化接口绑定 Provider、注册 Tool/Skill/Context/Policy；需要批量装配的能力包可在
包内实现普通函数，并自行处理失败回滚。只有出现可验证的共同生命周期需求后，才重新
评估事务式作用域；不能把本节早期草案当作现有接口。

## 16. 配置与装配

### 16.1 Core Config 与 Build Profile

本节按 ADR 0011 定义配置模型，并取代本文件早期关于运行期容量布局和
`agent_composition_t` 的草图。

Core 的模块裁剪和物理容量由 build Profile 决定，而不是由每个 `agent_config_t` 的资源字段
决定。Kconfig、CMake preset 或等价生成配置负责 Tool/Session/Context/Skill 是否编译，并生成
长期 registry/当前 session binding 容量、每轮 scratch 总预算和单对象准入上限。完整 Session
历史的保留、缓存与 I/O 预算属于 Session Storage Provider。输入、schema、arguments
和模型输出上限主要保护单次对象；它们不意味着每项都在 Core 中拥有同等大小的固定 buffer。

运行期 `agent_config_t` 只包含每个 Agent 可不同、不会改变 workspace 布局的内容：

```c
typedef struct {
    agent_string_view_t system_prompt;
    agent_limits_t limits;
    agent_runtime_t runtime;
} agent_config_t;
```

OpenAI、JSONL、Wi-Fi、Transport 和其他 Provider 配置不进入 `agent_config_t`。它们通过各自
Provider/Adapter API 创建并绑定，避免 Core 配置随着插件生态不断膨胀。

### 16.2 直接装配

首版不公开运行期 `agent_composition_t` 或可变 Profile builder。产品在当前 build Profile 下
创建 Agent 后，直接注册 Tool/Context/Skill、设置 Policy/Event 并绑定 Model。需要静态能力包
时，由包自身提供普通装配函数与失败回滚；通用 Plugin composition 待实际生命周期需求稳定后
再设计。

示例 Profile：

```text
Tiny Chat
├── Mock/Local Model
├── 1 Session
├── no persistent storage
└── small scratch

Device ReAct
├── OpenAI-compatible Model
├── HTTP Transport
├── Device Tool Pack
├── Confirmation Policy
└── JSONL/Flash Session Storage
```

## 17. 编译期内存 Profile

### 17.1 固定容量

每个 build Profile 在编译时确定 Core workspace 的大小、对齐和组成。Profile 的构建输出必须
报告 `sizeof(agent_workspace_t)`、启用模块和 Core 静态 RAM；这替代公开的运行期
容量查询与布局规划接口。

### 17.2 Caller-provided Workspace

```c
static agent_workspace_t g_agent_workspace;

agent_t *agent;
agent_error_t ret = agent_init(&agent, &g_agent_workspace, &config);
```

`agent_workspace_t` 由库保证对齐并随 build Profile 变化；应用与库必须使用同一生成配置。
`agent_init()` 初始化固定 registry、session binding/cursor 和可复用 turn scratch，不申请 Core heap。

公共 `agent/config.h` 以 `AGENT_*` 覆盖、`CONFIG_AGENT_*` 生成配置、Default 值的顺序
选择容量。`AGENT_CORE_WORKSPACE_BYTES` 定义 `agent_workspace_t` 的总大小；Core 必须在编译时
验证当前内部布局未超过它。Model wrapper 的 caller-storage 独立为
`agent_model_workspace_t`，不占用 Core workspace。

Host/大 SoC 保留便利封装：

```c
agent_t *agent_create(const agent_config_t *config);
void agent_destroy(agent_t *agent);
```

目标是 `agent_start()` 成功后，Core 主链路不再执行不可预测的 heap 分配。

### 17.3 内存区域

至少区分：

```text
Persistent Kernel State
Persistent Plugin State
Per-run Scratch Arena
Session Storage Provider Workspace and Persistent Media
Transport/Provider External Buffers
```

Provider 使用外部缓冲区时，必须在产品内存账中单独声明和测量；它不属于
`agent_workspace_t`。

## 18. 上层开发体验

### 18.1 直接装配接口

```c
static agent_workspace_t workspace;

agent_config_t config = agent_config_default();
agent_t *agent;

agent_init(&agent, &workspace, &config);
agent_register_tool(agent, &device_tool);
agent_set_policy_callback(agent, device_policy, NULL);
agent_set_model(agent, model);
agent_start(agent);
agent_run(agent, &request, &response);
agent_destroy(agent);
```

Tool/Session/Context/Skill 容量和 scratch 大小在此之前已由 build Profile 决定；Provider 和
Transport 按各自 API 创建，不进入 Core 初始化配置。

### 18.2 结构体注入（不提供 `_simple` 变体）

V2 不提供 `agent_create_simple()`、`agent_register_tool_simple()`、
`agent_run_text()` 之类“少填字段”的便捷函数；所有入口都要求调用者初始化完整结构体
再注入。C99 指定初始化器让这一步足够简洁：

```c
agent_register_tool(agent, &(agent_tool_t){
    .name = "get_time",
    .description = "Read current Unix time.",
    .execute = get_time_fn,
});

agent_request_t request = { .input = "hello" };
char output[512];
agent_response_t response = { .output = output, .output_size = sizeof(output) };
agent_run(agent, &request, &response);
```

未写字段归零，默认语义由类型定义保证（见 §14.1 的默认值规则），不需要额外的
`_simple` 包装。

### 18.3 字符串类型

跨模块和协议数据优先使用显式长度视图：

```c
typedef struct {
    const char *data;
    size_t size;
} agent_string_view_t;
```

配置中的长期静态字符串可以继续提供以 `NULL` 结尾的便利入口，但内部转换为带长度
视图，减少重复 `strlen()`、截断判断和二进制 payload 歧义。

## 19. 并发模型

V2 第一阶段采用明确的单写者模型：

- 一个 Agent 同时只运行一个 turn。
- 注册、卸载、Provider 替换和 Session 管理由同一任务串行执行。
- 其他线程只能调用明确标记为线程安全的 API，例如 cancel 和 stats snapshot。
- ISR 只能调用带 `_from_isr` 后缀的接口。
- callback 默认在驱动 turn 的线程中同步执行。
- callback 不得销毁 Agent、修改注册表或重入同一个 Agent。

不要用 `volatile` 代替同步。C99 构建通过 Runtime critical/atomic abstraction 实现
取消标志；如果未来要求 C11，可增加原子实现后端。

## 20. Event 模型

V2 区分两类事件。

### 20.1 Durable Session Event

表示需要支持恢复、重放或审计的事实，例如 user message、Tool call/result、final 和
turn abort。它们进入 Session log，并可由 Storage provider 持久化。

### 20.2 Live Runtime Event

表示当前运行过程中的观测信息，例如：

```text
MODEL_REQUEST_BEGIN
MODEL_REQUEST_END
TOOL_EXECUTE_BEGIN
TOOL_EXECUTE_END
TIMEOUT
CANCEL_REQUESTED
RESOURCE_PRESSURE
ERROR
```

Live event 默认不进入 Session log。Policy interceptor 与普通 observer 分离，避免
观察回调意外改变控制流。

## 21. Public Header 规划

建议公共头文件：

```text
include/agent/
├── agent.h
├── types.h
├── error.h
├── config.h
├── runtime.h
├── plugin.h
├── model.h
├── transport.h
├── tool.h
├── policy.h
├── skill.h
├── context.h
├── session.h
├── storage.h
├── memory.h
└── event.h
```

规则：

- 所有公共符号统一使用 `agent_` 前缀。
- 所有头文件（公共与内部）统一使用 `#pragma once` 作为包含守卫，不使用
  `#ifndef` 宏守卫。目标工具链（GCC、Clang、armclang/armcc、IAR）均已支持；
  若未来出现不支持的编译器或复制头文件的构建方式，迁移回宏守卫是机械操作。
- 每个公共头可以单独包含和编译。
- `agent.h` 是聚合头，但不自动包含厂商 Provider 和具体平台头。
- Mock API 放在单独的测试/Provider 头文件中。
- 内部头放在 `src/`，应用不可包含。
- 可演进 ops/config 使用 `abi_version` 和 `struct_size`。

## 22. 推荐目录结构

目录按"必需性"和"是否认识平台"两个维度分层。一条规则即可读出全部归属：

> `src/` 装每个构建都存在的 Kernel 机制；`providers/` 装可选的协议与格式实现（不认识
> 平台）；`ports/` 装可选的平台适配（认识 SDK）。同一功能的通用部分与平台部分分居
> `providers/` 与 `ports/`，ADR 0029 的两层加载器结构即按此落地。

```text
cAgentV2/
├── include/agent/     Kernel 唯一公共入口；不随 Profile 变化
├── src/               Kernel：每个构建都存在，无平台依赖
│                      生命周期、workspace、session 事务、model wrapper、
│                      skill registry（机制）、arena、cancel、error、event
├── codecs/json/       可选：有界 JSON 读写器，被多个协议实现共用
├── providers/         可选：协议与格式实现，不认识平台
│   ├── model/openai      Chat Completions 非流式
│   ├── model/mock        测试用脚本化 completion（占位）
│   ├── storage/jsonl     Session 记录格式
│   ├── storage/ram       易失后端
│   └── skill/loader      Markdown + front-matter 加载（ADR 0029 通用层，未实现）
├── ports/             可选：平台适配
│   ├── posix/storage     Session 文件适配
│   ├── posix/skills      Skill 目录枚举与定长读（ADR 0029 薄层，未实现）
│   ├── espidf/           runtime、transport、storage
│   └── openvela/         runtime、transport、storage
├── tests/  docs/  examples/
└── CMakeLists.txt
```

两条归属判据，用于新增模块时判断落点：

1. **`src/` 只收"每个构建都存在"的机制。**可选模块不进 `src/`，否则构建裁剪、代码
   体积为零的承诺（如未注册加载器时代码体积为零）与 ABI 边界都会松动。Skill 加载器
   是可选格式实现，因此归 `providers/`；`src/skill/` 只保留注册表机制，是加载器的
   下游而非其实现位置。
2. **认识的平台 API 决定 `providers/` 还是 `ports/`。**通用逻辑层不触碰平台 SDK、可在
   Host 完整测试，归 `providers/`；枚举、定长读、路径与错误映射归 `ports/`。同一功能
   的两层分居两地是设计结果，不是冗余，合并才是有问题的。

Session 命名空间与 Skill 注册语义由各自领域组件维护。共享文件契约、预制平台
实现及接入步骤以 [ADR 0031](adr/0031-prefabricated-platform-file-storage.md) 为准，
用 JSONL Session 与 USER 文件有界读取验证；可选通用组件统一放在 `providers/`。

ESP-IDF/OpenVela Adapter 已有 mock 测试，但尚未经真实网络/TLS 与文件系统掉电恢复的
集成验证；上图不表示所有列出的模块已经实现。

当前通用构建目标为 `cagent_core`；Host CMake 可选构建 `cagent_provider_mock` 和
`cagent_provider_openai` 占位目标，Anthropic 尚无目标。具体协议实现与 Provider
配置头以后在各自目录内完成；不能把空目标视为可用的云模型接入。

## 23. V1 迁移决策

| V1 能力 | V2 处理方式 |
|---------|-------------|
| 不透明 `agent_t` | 沿用 `agent_t` 命名，内部结构重新设计 |
| 错误码分区 | 保留并统一语义 |
| Runtime callback | 保留，拆分 Transport |
| Tool/Skill/Context 注册 | 保留概念，加入作用域和事务 |
| Session turn 完整性 | 保留并转为事件状态机 |
| 固定二维 Session 数组 | 改为 Session Storage Provider 的记录/缓存与有界投影 |
| 同步 `agent_run()` | 保留为状态机便利封装 |
| request 覆盖 Agent limits | 改为独立 run context |
| Model 请求中的 JSON | 移出 Core，由 Provider 构造 |
| Provider 直接 malloc/free | 改为统一 allocator 或 caller state |
| Memory 空实现 | 不直接迁移，契约明确后再加入 |
| Core 中 fopen/getenv | 移到 POSIX integration/plugin |
| 空 Router 骨架 | 不迁移 |
| 未生效标志和字段 | 不迁移，先定义行为 |
| 手写 OpenAI parser | 通过测试后重写或替换 |

V1 代码是行为参考，不是逐文件复制来源。迁移前先为目标行为建立 characterization
tests，避免把已知问题一并复制到 V2。

## 24. 分阶段实现计划

### 阶段 0：契约和公共头草案

交付：

- 本架构文档；
- ownership matrix；
- error model；
- lifecycle state；
- `agent_config_t` 草案；
- `agent_plugin_scope_t` 草案；
- `agent_run_context_t` 草案；
- memory plan 草案；
- Session event schema。

本阶段不复制 V1 业务实现。

阶段 0 的原始计划已过时：同步运行与取消声明现位于 `agent.h`，独立 `run.h` 已移除；
当前实现进度以文档索引和实际测试结果为准。

### 阶段 1：最小 Kernel

实现：

```text
runtime validation
workspace allocator
memory planner
lifecycle
plugin scope
typed registry
error/stats
strict build and unit-test harness
```

在编码前补充并验收 Runtime port 的最小 capability contract，以及 Model/Transport
选择同步最小接口还是 `start/poll/cancel` 非阻塞接口的 ADR。

验收：所有初始化失败点能够逆序清理，Core 可以在 caller-provided workspace 中启动
和销毁，销毁后没有残余注册。

### 阶段 2：Session、Mock Model 和默认 Loop

先跑通：

```text
user -> mock model -> final
```

再跑通：

```text
user -> mock tool call -> tool result -> mock final
```

验收：Session 事件可投影成规范化消息，完整 turn 可以淘汰，失败生成 abort 并保持
状态一致。

### 阶段 3：Policy、确认和可驱动状态机

实现：

- Tool pipeline；
- Policy chain；
- `WAITING_CONFIRMATION`；
- cancel 和 deadline；
- live events；
- 同步 `agent_run()` 封装。

验收：危险 Tool 在没有确认机制时不会执行；暂停、恢复、取消和失败路径均有测试。

### 阶段 4：Transport 和 OpenAI Provider

实现：

- Transport capability；
- POSIX/mock transport；
- OpenAI-compatible serializer/parser；
- response sink；
- 截断、转义、Unicode、异常响应测试；
- parser fuzz harness。

验收：Core、Session 和 Tool 代码中不包含 OpenAI wire-format JSON 构造逻辑。

### 阶段 5：Session Storage

实现：

- versioned record codec；
- JSONL backend；
- append/checkpoint/sync；
- truncated-tail recovery；
- list/open/remove/resume；
- Session fork 的数据基础。

验收：一次运行可在进程/设备重启后恢复，损坏尾部不会破坏此前完整 turn。

### 阶段 6：真实平台与 V1 兼容

实现：

- openvela port 和 Transport；
- ESP-IDF/RT-Thread/STM32 边界；
- V1 compatibility adapter；
- Tiny、Default、Device ReAct profiles；
- 完整示例、CI 和安装规则。

验收：V1 与 V2 可使用同一组行为测试比较结果，并记录 RAM、ROM、运行峰值和协议
差异。

## 25. 测试策略

### 25.1 必须覆盖

- 每一个初始化和插件挂载失败点；
- workspace 刚好足够和少一个字节；
- allocator callback 配对错误；
- Plugin scope commit/abort/unmount；
- Model owned/borrowed/transferred 生命周期；
- Session turn、Tool call/result、abort、淘汰；
- Context critical/normal/optional 预算；
- Policy allow/deny/confirm 合并；
- cancel、timeout、busy 和 callback 重入；
- 输出 buffer 不足和 required size；
- JSON 转义、嵌套、Unicode、截断和超容量；
- Tiny/Default/Profile 构建矩阵；
- 公共头文件独立编译。

### 25.2 工具

Host 测试至少启用：

```text
-Wall -Wextra -Wpedantic -Werror
AddressSanitizer
UndefinedBehaviorSanitizer
parser fuzzing
clang-format check
```

目标平台增加静态内存占用和栈深度报告。

## 26. 首批架构决策建议

在编写实现前，应分别形成 ADR：

1. V2 使用 C99，原子操作由 Runtime abstraction 提供。
2. V2 公共符号统一使用 `agent_` 前缀。
3. V2 Core 使用 caller-provided workspace，`create()` 只是便利封装。
4. Runtime 是创建时不可替换的基础能力，Transport 是可组合 Provider。
5. Session 使用有界事件日志，Model history 通过 projection 得到。
6. Model API 使用 typed views 和 response sink，不传递厂商 JSON。
7. Plugin 是静态生命周期单元，第一阶段不支持运行中热卸载。
8. Plugin 注册通过 scope 实现事务和自动逆序撤销。
9. Core config 不包含具体 Provider 配置。
10. 默认 Loop 内部使用状态机，同步 API 只做驱动封装。
11. 不提供 `_simple` 便捷函数，所有入口使用完整结构体注入；`flags == 0` 表示安全默认。

## 27. 近期工作顺序

建议接下来依次编写：

1. `docs/adr/0001-language-and-abi.md`
2. `docs/adr/0002-workspace-memory-model.md`
3. `docs/adr/0003-plugin-capability-model.md`
4. `docs/adr/0004-session-event-model.md`
5. `docs/adr/0005-run-state-machine.md`
6. `include/agent/types.h`
7. `include/agent/error.h`
8. `include/agent/runtime.h`
9. `include/agent/config.h`
10. `include/agent/plugin.h`

公共头草案评审通过后，再开始 Kernel 实现。

## 28. 总结

cAgentV2 不应复刻一个嵌入式版本的通用动态插件框架。它应该组合以下经验：

```text
类型化 Capability Seam
+ 可逆插件作用域
+ cAGENT V1 已验证的有界 ReAct/Session 规则
+ 嵌入式静态链接与 workspace
+ 有界、可恢复的 Session 事件
+ 可驱动运行状态机
```

最终目标不是“所有东西看起来都像插件”，而是：

> 所有可变能力都能在不修改 Kernel 的情况下被类型安全地替换或组合，同时保持
> 生命周期可证明、失败可回滚、内存可计算、运行可追踪。
