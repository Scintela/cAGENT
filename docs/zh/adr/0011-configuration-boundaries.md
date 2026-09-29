# ADR 0011: 编译期容量 Profile、运行期配置与可替换能力边界

- 状态：提案
- 日期：2026-09-26

## 背景

cAgentV2 的主要目标是 ESP-IDF、openvela、RT-Thread 等由产品固件静态链接源码的嵌入式
场景。此类产品通常在构建时已知需要哪些模块、一个 Agent 需要多少 Tool/Session/Context
槽位，以及可为 Core 保留多少 RAM。为每个 Agent 暴露大量运行期容量字段和布局查询接口
虽然灵活，却增加公共 API、初始化布局代码和应用配置负担，未必符合首版轻量 Core 的目标。

cAgentV1 使用 Kconfig/编译期宏约束长期容量，但存在两个独立问题：最大数组直接焊入
`agent_t`，且每次 run 另申请 request arena。前者不等于“编译期容量”本身有问题，后者则
破坏了 Core 运行期内存确定性。V2 应保留编译期 Profile 的可预测性，同时使用共享 payload
pool 与可复用 scratch 避免 V1 的固定二维数组和每轮 heap arena。

Model、Transport、TLS、HTTP 和 JSON Provider 的资源、所有权及共享方式也与 Core workspace
不同。将具体 Model 或协议缓冲直接嵌入 Core 初始化配置会混淆 provider 生命周期和 Core RAM
账本。

## 决定

### 容量与模块裁剪在编译期确定

Core 模块是否编译以及其容量上限由构建系统的 Kconfig、CMake cache 或等价的生成配置头决定。
公共 `agent/config.h` 按下列优先级取得容量：直接定义的 `AGENT_*`、生成的
`CONFIG_AGENT_*`、内建 Default 值。构建 Profile 应生成 `CONFIG_AGENT_*`；直接 `AGENT_*`
覆盖仅用于源码集成、测试或产品临时配置。普通应用只选择 Tiny、Default、Device ReAct 或
产品自定义 Profile，不应被要求理解和填写全部容量项。Profile 内部至少覆盖：

```text
CONFIG_AGENT_ENABLE_TOOL / SESSION / CONTEXT / SKILL
CONFIG_AGENT_MAX_TOOLS
CONFIG_AGENT_MAX_CONTEXTS
CONFIG_AGENT_MAX_SKILLS
CONFIG_AGENT_SCRATCH_BYTES
CONFIG_AGENT_MAX_PROJECTED_MESSAGES
CONFIG_AGENT_MAX_INPUT_BYTES
CONFIG_AGENT_MAX_CONTEXT_BYTES
CONFIG_AGENT_MAX_SCHEMA_BYTES
CONFIG_AGENT_MAX_ARGUMENTS_BYTES
CONFIG_AGENT_MAX_TOOL_OUTPUT_BYTES
CONFIG_AGENT_MAX_MODEL_OUTPUT_BYTES
CONFIG_AGENT_MAX_MODEL_TOOL_CALLS
CONFIG_AGENT_MAX_NAME_BYTES
CONFIG_AGENT_MAX_DESCRIPTION_BYTES
CONFIG_AGENT_MAX_IDENTIFIER_BYTES
CONFIG_AGENT_MAX_JSON_DEPTH
CONFIG_AGENT_CORE_WORKSPACE_BYTES
```

未启用的模块不进入链接依赖和 Core workspace；启用模块的槽位、scratch 与输入边界由同一
build Profile 固定。动态注册和远程 Tool catalog 只能在对应编译期容量内发生，超出时返回
`AGENT_ERROR_LIMIT`，不得 heap fallback 或无界扩容。Session 历史的保留与缓存属于独立的
Session Storage Provider，不计入 Core workspace，详见 ADR 0016。

Profile 不能只提供一个裸的 `CONFIG_AGENT_CORE_WORKSPACE_BYTES`。总字节数不能表达这块 RAM
应该优先留给 registry、当前 turn 控制状态还是 context；若由库在运行时猜测，注册顺序和业务负载会改变
同一 32 KB Profile 的可运行性。当前容量宏按四类解释：

| 类别 | 当前宏 | 内存与生命周期 | 配置责任 |
|---|---|---|---|
| Persistent capacity | `MAX_TOOLS`、`MAX_CONTEXTS`、`MAX_SKILLS` | 固定 registry 槽位、Agent 生命周期状态和当前 session binding/cursor，跨 turn 存活。 | Core Profile 的核心输入；Custom 时才逐项覆盖。 |
| Turn scratch budget | `SCRATCH_BYTES`、`MAX_PROJECTED_MESSAGES` | 一个活动 turn 的可复用 arena；context、消息/Tool view 数组、arguments、临时 Tool result 和模型运行事实按需共享，turn 结束整体 rewind。 | `SCRATCH_BYTES` 是核心输入；message 数量是防止 view 数组无界增长的硬上限。 |
| Per-object admission cap | `MAX_INPUT_BYTES`、`MAX_CONTEXT_BYTES`、`MAX_SCHEMA_BYTES`、`MAX_ARGUMENTS_BYTES`、`MAX_TOOL_OUTPUT_BYTES`、`MAX_MODEL_OUTPUT_BYTES`、`MAX_MODEL_TOOL_CALLS`、名称/描述/标识符长度及 `MAX_JSON_DEPTH` | 限制一次输入、一个对象或一次响应，防止其耗尽对应 pool 或 scratch；通常不单独预分配同等大小的固定 buffer。 | Profile 默认值；仅 Custom/Advanced 配置开放。 |
| Generated workspace size | `CORE_WORKSPACE_BYTES`；通用 Model wrapper 的 `MODEL_WORKSPACE_BYTES` | Core 长期区、scratch、控制状态和对齐后的总 caller-storage；Model wrapper 是独立且很小的 caller-storage。 | 由 Profile/库生成，应用使用类型而非手填字节数。 |

其中 Tool schema、名称和描述当前都是 borrowed view；例如 `MAX_SCHEMA_BYTES` 是注册/解析准入
阈值，不表示 Core 为每个 schema 常驻分配同等字节。完整 OpenAI tools JSON 的序列化 buffer 属于
Model Provider workspace。若 Core 的 codec 需要 schema/arguments 的临时解析状态，该状态计入
scratch，但不能把借用 schema 本体误算为 Core 持久或 scratch buffer。

`MAX_SESSIONS`、`SESSION_EVENT_CAPACITY` 和 `SESSION_PAYLOAD_BYTES` 仍出现在当前过渡性头文件中，
但在持久化 Session 目标模型中，它们应迁移为可选 RAM Session Storage Provider 的 Profile
配置，而不是 Core workspace 的布局输入。在 Storage Provider 接口和该迁移尚未实现前，当前
头文件不应被理解为已承诺的持久化 Session ABI。

因此总 Core workspace 大小应由前三类中实际常驻布局和最大临时峰值推导，或由构建系统同一处生成。
单项上限不能仅靠总 scratch 取代，否则一个过大的 arguments、Tool output 或模型输出可以耗尽
本轮其他必要临时空间。

建议维护 Tiny、Default、Device ReAct 等构建 Profile，而不是让 `agent_config_tiny()` 在运行期
改变物理容量。Profile 可以由各平台的 Kconfig choice、CMake preset 或产品配置文件选择；
同一个静态库构建只对应一组 Core 容量。

### 使用类型化固定 workspace

公开 API 应提供由当前编译 Profile 决定大小和对齐的 `agent_workspace_t`，而不是要求应用猜测
`unsigned char[]` 的长度或对齐：

```c
static agent_workspace_t g_agent_workspace;

agent_t* agent;
agent_error_t rc = agent_init(&agent, &g_agent_workspace, &config);
```

当前公开实现为带 pointer、`uint64_t` 和 `long double` 对齐成员的 union，并以
`unsigned char bytes[AGENT_CORE_WORKSPACE_BYTES]` 承载私有 Core 存储。Core 实现必须在编译时
验证当前内部布局不超过 `AGENT_CORE_WORKSPACE_BYTES`。`agent_init()` 负责清零/绑定 workspace、
初始化固定 pool 和进入 CONFIGURING；它不在主路径申请 heap，也不需要在运行时计算不同资源布局。

因此首版不提供公开的运行期内存规划 API。布局计算职责在编译期由 Profile 和
`sizeof(agent_workspace_t)` 承担。Host 或明确允许 heap 的构建可保留
`agent_create()`，其分配大小同样是 `sizeof(agent_workspace_t)`，而非根据运行期资源字段推导。

`agent_workspace_t` 是该收敛后的唯一 Core caller-storage 入口；不得再引入带运行期容量表或
raw `void* + size` 的并行初始化路径。

### workspace 内部使用固定预留与弹性 scratch

应用提供一个连续的 `agent_workspace_t`，但 Core 不把所有临时对象做成固定最大数组。
`agent_init()` 按 Profile 先建立长期状态区，再建立可复用的 turn scratch arena：

```text
agent_workspace_t
|-- Core private state and lifecycle data
|-- registry descriptors, session binding/cursor  (长期预留)
`-- turn scratch arena                            (每轮弹性复用)
    |-- context projection
    |-- message / Tool view arrays
    |-- Tool arguments and temporary results
    `-- bounded model-output facts
```

一个 turn 内，context 可使用未被消息/Tool view 数组、arguments 或 Tool temporary result 使用的
scratch；turn 结束后 arena 整体 rewind，下一 turn 从干净边界重新开始。这样临时内容按实际负载
共享空间，同时总峰值保持确定。借用 schema 本体不在此处复制；其大小由注册准入和 Provider
workspace 分别约束。

长期 registry 或 session binding 分配不得在运行中从 turn scratch 持久占用空间；否则“后来注册一个
Tool”可能让下一轮原本可运行的 context 失败。单项临时对象超过自身上限或 scratch 剩余空间时，
Core 必须返回明确的容量错误，不能压缩长期区、静默截断或申请 heap。

### `agent_config_t` 只保留运行期初始化值

迁移后的 `agent_config_t` 不携带 Core 容量，而只包含每个 Agent 实例可不同、不会改变固定
workspace 布局的值：

```c
typedef struct {
    agent_string_view_t system_prompt;
    agent_limits_t limits;
    agent_runtime_t runtime;
} agent_config_t;
```

| 字段 | 职责 | 初始化后是否可改变 |
|---|---|---|
| `system_prompt` | 固定基础指令的借用 view | 否；重新配置需新建 Agent。 |
| `limits` | 后续 turn 的默认行为预算 | 是；仅 CONFIGURING/READY。 |
| `runtime` | 时间、可选 allocator、取消同步和日志服务 | 否。 |

`agent_init()` 复制 config 的值类型字段，但不取得任何借用对象的所有权。提示文本、Runtime
context、allocator、clock、sync 与 log state 均须覆盖 Agent 生命周期。Core 不在 init 中打开
网络、创建应用线程或发起模型请求。

`agent_config_default()` 只提供当前 build Profile 下的默认运行值；不再有暗示运行期容量
Profile 的 `agent_config_tiny()`。若保留类似 helper，它只能调整 limits，名称必须避免暗示
会缩小已编译 workspace。

### 行为 limits 继续在运行期管理

`agent_limits_t` 仍是每个 Agent 与每个 turn 的运行预算，包含模型迭代、deadline、工具调用
次数和模型 token 请求。`agent_request_t.limits == NULL` 继承 Agent 默认值；非 NULL 时提供
完整覆盖，不采用零值逐字段继承。

运行期 limits 不得突破 build Profile 的硬边界。例如：`max_tool_calls` 不能高于可安全记录的
调用事实容量，模型/Tool 输出 token 请求不能绕过编译期字节上限，未编译 Tool 模块时任何
非零 Tool 调用预算均不使 Tool 可用。实现应在 begin 或调用边界拒绝不一致配置，而不是静默
截断或临时扩容。

### Model 不属于初始化配置

`agent_config_t` 不包含 Model、Transport 或厂商 endpoint。Model 是可替换外部 Provider，通过
独立 binding API 接入：

```c
agent_error_t agent_set_model(agent_t* agent, agent_model_t* model);
agent_error_t agent_set_model_owned(agent_t* agent, agent_model_t* model);
agent_model_t* agent_get_model(const agent_t* agent);
```

绑定仅允许 CONFIGURING/READY；任何 active turn，包括等待 Model、Tool 或确认时，替换返回
`AGENT_ERROR_BUSY`。`agent_set_model()` 是 borrowed binding，`agent_set_model_owned()` 仅在
成功时转移 wrapper 所有权。替换必须先验证新 binding；失败时旧 binding 不变，成功后旧
borrowed binding 只解除引用，旧 owned binding 按其销毁契约清理。

首版允许无 Model 的 READY，供 Tool/Session/Context 装配和测试使用；需要模型的 run/turn
在使用点检查 binding。按请求选择云端、本地或故障转移模型应由 Router Model Provider 在
一次 `complete()` 内完成，不得 active turn 中替换全局 binding。

Model wrapper、OpenAI request/response JSON、cJSON DOM、HTTP/TLS buffer、连接池、网络任务
栈和持久化存储均不属于 `agent_workspace_t`。它们由 Provider、Transport 或应用独立配置、
预算和测量，见 ADR 0006 与 ADR 0007。

`agent_model_workspace_t` 只容纳通用 Model wrapper，大小由
`AGENT_MODEL_WORKSPACE_BYTES` 决定；它同样不容纳具体 Provider 状态。需要无 Core heap 的
Provider 作者可为自己的状态公开独立的类型化 workspace，或由产品明确分配该状态。

## 不采用的方案

### 每个 Agent 使用运行期容量表与布局查询

该方案适合预编译二进制 SDK、同一进程中多个不同容量实例或运行期组合插件。首版目标是
固件源码库和构建期 Profile；为这些较少见需求引入动态 layout 和大配置表不划算；拒绝。

### 只保留 V1 的固定二维数组与每轮 request arena

会重复 V1 的最大值常驻浪费与运行期 heap 峰值问题。V2 使用 build Profile 固定容量，但内部
采用固定 descriptor pool 与可复用 turn scratch；Session 历史由 Storage Provider 管理；拒绝。

### 把运行 limits 合并进编译期容量宏

timeout、最大步骤、每请求 Tool 禁用和 token 请求是运行行为。它们需要按 Agent 或 request
调整，不能因改动 timeout 而重编固件；拒绝。

### 将 Model 或 Transport 嵌入 `agent_config_t`

会把 Provider 创建、外部内存、连接、共享和替换语义带入 Core 初始化路径；拒绝。

### 运行中热替换 Model

会使同一个 turn 的请求和 Tool result 进入不同 Provider，且取消、超时和所有权无法可靠处理；
首版拒绝。

## 影响

正面影响：

- 固件构建时可审查 Core 的 ROM/RAM 上限，应用不需要填写容量表或调用预检 API；
- `agent_init()` 成为简单、确定的 caller-workspace 绑定操作；
- 一个 build Profile 内没有 Core 运行期 heap 扩容和每轮 arena 创建；
- Kconfig/CMake 可以同时裁剪未使用模块、Provider 依赖和容量；
- Model/Transport/JSON 仍保持可替换、可共享且不污染 Core workspace。

代价：

- 一个静态库构建只能有一组 Core 容量；不同产品或不同 Agent 尺寸需要重建 Profile；
- 同一固件中的多个 Agent 通常须接受相同容量，除非未来引入独立的多 Profile 构建机制；
- `agent_workspace_t` 是与 build Profile 绑定的 ABI/源码契约，应用与库必须使用同一生成配置；
- 发布预编译通用二进制 SDK 时，运行期容量模型可能需要作为未来可选 Profile 重新评审。

## 评审重点

1. `agent_workspace_t` 是否作为唯一 caller-storage API；当前结论是“是”，以消除 C99 对齐和
   大小猜测问题。
2. 是否保留 `agent_create()`；当前结论是“保留为 Host/heap convenience，不作为 MCU 主路径”。
3. `agent_config_tiny()` 是否完全删除；当前倾向“删除，用 build Profile 取代”。
4. `agent_limits_t` 的哪些字段需要 build-time hard ceiling；当前结论是所有可能导致 Core
   capture/记录越界的字段都必须受编译期容量约束。
5. 将来是否为 Host 预编译 SDK 重新引入运行期 layout；当前结论是“可以作为
   另一 build mode，不污染 MCU 首版主 API”。

## 验证要求

- 每个 Tiny/Default/Device ReAct build Profile 都报告 `sizeof(agent_workspace_t)`、链接模块和
  Core 静态 RAM；
- 未启用模块的 API 注册路径明确拒绝或不编译，不引入 heap fallback；
- `agent_init()` 在各 Profile 上不分配 heap，并正确初始化固定 pool/scratch；
- Tool/Session/Context/Model 输出、schema、arguments、JSON 深度等所有 build-time 上限都有
  成功、恰好上限、超出上限测试；
- request limits 的继承、完整覆盖、非法 max steps、deadline 和 Tool 禁用均有测试；
- CONFIGURING/READY 中 Model borrowed/owned 绑定与原子替换符合所有权契约，ACTIVE 替换返回
  `AGENT_ERROR_BUSY`；
- Provider/Transport/JSON/TLS/RTOS 的外部内存峰值与 Core workspace 分别报告。
