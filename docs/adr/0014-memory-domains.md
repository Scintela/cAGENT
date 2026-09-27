# ADR 0014: Core、Provider 与外部库的内存域

- 状态：提案
- 日期：2026-09-26

## 背景

cAgentV1 通过 Kconfig/编译期宏限制 Tool、Session 和 buffer 的最大值，但完整 `agent_t`
在 `agent_create()` 时从 heap 分配，每次 `agent_run()` 又创建和销毁 request arena。OpenAI
Provider 还单独为 request body、response body 以及网络/TLS 资源申请内存。因而 V1 的容量
有界不等于运行期分配可预测。

V2 的首要目标是让 ESP-IDF、openvela、RT-Thread 等产品能独立审查 Core RAM，并在正常 turn
路径中避免 Core 触发 heap 分配。与此同时，Model、HTTP、TLS、JSON 和应用 Tool 的内存需求
高度依赖具体实现，不能伪装为 Core 固定内存的一部分。

ADR 0011 已决定以 build Profile 和 `agent_workspace_t` 取代运行期容量布局。本文进一步规定
每一类内存的所有者、生命周期、可使用的分配方式和可验证边界。

## 决定

### 内存域按所有者划分

```text
Application
|
|-- Core workspace: agent_workspace_t
|   |-- private Agent state and lifecycle state
|   |-- Tool / Context / Skill registry descriptors
|   |-- active turn, session binding/cursor and cancellation state
|   `-- reusable Core scratch arena
|
|-- Session Storage Provider workspace: provider-specific
|   |-- session index and metadata cache
|   |-- PSRAM hot-history cache and read/write staging
|   `-- Flash/NVS/JSONL codec and checkpoint state
|
|-- Model Provider workspace: provider-specific
|   |-- provider state
|   |-- request serialization buffer
|   |-- response capture buffer
|   `-- codec/token scratch
|
|-- Transport workspace: transport-specific, optional
|   |-- HTTP framing and I/O staging buffers
|   `-- TLS / connection state
|
|-- Application Tool state
|   `-- device handles, driver buffers and business state
|
|-- Persistent media: optional
|   `-- Flash / NVS / filesystem JSONL session history
|
`-- External libraries
    `-- heap only when the selected implementation explicitly permits it
```

应用拥有所有 workspace 的放置和生命周期，可根据目标将它们放在 `.bss`、内部 SRAM、PSRAM
或其他受控内存区。Core 不拥有 Application、Provider、Transport 或 Tool 状态。

### Core caller-storage 是唯一的 Core RAM 容器

`agent_workspace_t` 是当前 build Profile 下的完整 Core caller-storage。它包含 Core 的
`agent_t` 私有状态、registry、当前 session binding/cursor、turn 状态和 scratch；它不包含完整
Session 历史、event descriptor ring 或 payload pool。

Session Storage Provider 有自己的 workspace 或受控 allocator。其缓存、写入缓冲、索引和
checkpoint 状态由 Provider 或应用单独预算；Flash/NVS/filesystem 是持久化介质，不是 workspace。
Core 对 Storage Provider 发出有界查询并得到 callback-lifetime history view，详见 ADR 0016。

`agent_workspace_t` 的大小由 `AGENT_CORE_WORKSPACE_BYTES` 决定。Core 实现必须在编译期检查
实际内部布局不超过该上限；不得以 heap fallback 补偿 Profile 设置不足。

`AGENT_CORE_WORKSPACE_BYTES` 是 Profile 的总上限，不是唯一资源配置项。Core 在 init 时必须把
workspace 划分为两个不互相侵蚀的域：跨 turn 存活的固定预留域，以及仅供一个活动 turn 使用的
scratch 域。Tool/Context/Skill registry、session binding/cursor 与生命周期状态位于前者；context、
消息/Tool view 数组、Tool arguments、临时 Tool result 与运行事实位于后者。Storage Provider 的
历史增长不会在运行中偷占 Core context 的工作空间。

### Core 在 caller-workspace 路径中不得使用 heap

对于下列 API，Core 不得调用 `agent_runtime_t.allocator.alloc`，也不得间接创建无界容器：

| 操作 | Core heap 规则 | 内存来源 |
|---|---|---|
| `agent_init()` | 禁止 | caller `agent_workspace_t` |
| `agent_start()` | 禁止 | 已初始化的固定 pool |
| Tool/Context/Skill 注册与注销 | 禁止 | 固定 registry slots |
| Session 创建、追加、查询、淘汰、清除 | 禁止 | Core 控制状态和 Session Storage Provider 的独立资源 |
| Policy、Event observer、limits 更新 | 禁止 | 固定 Agent 状态或栈上临时值 |
| `agent_run()` 与显式 turn API | 禁止 | reusable Core scratch 与 caller output buffer |
| `agent_destroy()` | 禁止释放 caller storage | 清理引用和 owned 外部资源 |

`agent_create()` 是明确的 Host/heap convenience API：它可通过 Runtime allocator 分配
`sizeof(agent_workspace_t)`，再走与 `agent_init()` 相同的初始化逻辑。它不是 MCU 主路径，且
不能被用于证明 caller-workspace 路径会分配 heap。

### Core scratch 只服务一个活动 turn，并在其中弹性复用

Core scratch 是 workspace 内可复用的线性 arena，而不是每轮申请的 arena：

```text
turn begin -> mark
  -> context projection
  -> message/tool projection
  -> bounded model output and Tool-call facts
turn end   -> rewind to mark
```

scratch 内的对象只在其 owning turn 或 callback 内有效。Storage Provider 返回的历史 view、注册项、
已确认 Tool 结果等跨 turn 数据不得指向 scratch。Core 必须使用显式 `AGENT_ERROR_CAPACITY`、
`AGENT_ERROR_CONTEXT_OVERFLOW` 或相应模块错误报告容量不足，不能静默截断或临时扩容。

scratch 是一个受限 arena，不要求为 context、message/Tool view 数组、arguments、Tool output
分别配置等大的固定数组。本轮未使用 arguments 或临时 Tool result 时，context 可以使用相应的
剩余 scratch；反之亦然。注册的 Tool schema 是 borrowed view，`MAX_SCHEMA_BYTES` 主要是
注册/解析准入阈值，完整 schema 的 Provider 序列化 buffer 不属于 Core scratch。Profile 仍应给
单项内容配置硬上限，防止一个异常大的 arguments、Tool output 或模型输出独占全部 scratch，
使其他必要临时对象无法写入。普通产品通过选择 Profile 获得默认值；只有 Custom/Advanced
Profile 才需要直接调整细项。

### Event 默认同步；异步队列必须预分配

Core 默认以同步 observer callback 发送临时 `agent_event_t` view。Core 不为一次 event 分配
payload，observer 不得保留 callback-lifetime view。

若产品需要跨线程事件投递、可靠事件日志或 UI 队列，该队列属于 Application 或可选 Event
Adapter。它必须使用预分配 ring/payload pool，并自行规定溢出、丢弃、阻塞与线程安全策略。
Core 不因异步消费需求引入隐式 event heap。

### Model wrapper 不等于完整 Provider 内存

当前 `agent_model_workspace_t` 只容纳通用 `agent_model_t` wrapper，大小由
`AGENT_MODEL_WORKSPACE_BYTES` 决定。它不包含 OpenAI 请求 JSON、响应 JSON、codec token、
HTTP body、TLS 状态或连接池。

具体 Provider 应按自身协议公开独立的类型化 workspace 或 caller-buffer 配置。例如下列类型
仅为目标形态，不是当前已存在 API：

```c
static agent_openai_workspace_t g_openai_workspace;
static agent_transport_espidf_workspace_t g_transport_workspace;

agent_openai_model_init(&model, &g_openai_workspace, &openai_config);
agent_transport_espidf_init(&transport, &g_transport_workspace, &transport_config);
```

这使产品可以把 Core 放在内部 SRAM，把较大的 JSON/HTTP/TLS buffer 放在 PSRAM，并分别测量。
如 `agent_model_workspace_t` 的名称持续造成“包含完整模型内存”的误解，应在 ABI 稳定前另行
评审重命名为 `agent_model_wrapper_workspace_t`；本 ADR 不在此刻修改公开符号。

### Provider 与外部库必须声明分配行为

Core 只承诺自身 allocation-free。Provider、Transport 和 Tool 必须在其文档中至少声明下列
三种模式之一：

| 模式 | 含义 |
|---|---|
| fixed workspace | 所有状态和 buffer 均由 caller workspace/caller buffers 提供。 |
| bounded allocator | 使用显式、受限的 allocator/pool，并能报告上限和失败。 |
| external heap | 第三方库可能调用 heap；必须说明触发阶段、峰值测量方法和失败传播。 |

全链路零 heap 只能由具体产品选择 fixed-workspace Provider、Transport、JSON codec、TLS 配置和
Tool 实现后达成，不能由 Core 单独承诺。cJSON 默认 allocator、mbedTLS、DNS、HTTP SDK 以及
业务 Tool 的内部行为均在 Core 边界之外。

### Core Gateway 只交换规范化 view 和 sink

Core 到 Model 的 Gateway 只构造 `agent_model_request_t`，传递消息、Tool、Context 等有界 view，
并通过 `agent_model_sink_t` 接收文本和 Tool call。它不得生成厂商 JSON、管理 HTTP/TLS、持有
socket、创建网络线程或向 Provider 借用 Core scratch 作为长期 buffer。

这样 Model Provider 可以替换为云端、本地、Mock 或 Router，而 Core 的内存模型不随协议改变。

## 不采用的方案

### 将 Model、Transport 和 TLS 全部装入 Core workspace

会迫使不用网络的产品承担网络状态，且 OpenAI、本地模型、串口网关等 Provider 的内存布局完全
不同；Core 容量不再可独立审计，拒绝。

### 为每个 Core 模块公开独立 workspace

Tool、Context、Skill、turn 都是一个 Agent 内部维护不变量的组成部分。暴露多个 Core caller
workspace 会使初始化顺序、对齐、失败回滚和所有权更复杂，且不能带来真正的共享收益；拒绝。
Core 对外只暴露一个 `agent_workspace_t`。Session Storage Provider 是外部可替换能力，因此可以
拥有独立 workspace。

### 复用 V1 的每轮 request arena

即使 arena 内部是线性分配，若每次 turn 都从 heap 创建和销毁，它仍产生运行期分配失败和峰值
抖动；拒绝。

### 宣称整个 Agent 系统天然零分配

Provider、TLS、JSON、外部网络栈与 Tool 行为不受 Core 控制。此类承诺不可验证且会掩盖产品
真实内存风险；拒绝。

## 影响

正面影响：

- Core RAM、Session Storage 历史/缓存、turn 临时峰值和 Provider/TLS 峰值可分开测量；
- caller-workspace 主路径在正常运行中没有 Core heap 失败点、碎片或时延抖动；
- Session 历史不再被误设计为 Core 常驻 RAM，而仍由 Core 维护完整 turn 与 Tool 配对不变量；
- Provider 可按平台和协议选择内部 SRAM、PSRAM、固定 pool 或受控 heap；
- Model Gateway 保持协议无关，避免 OpenAI HTTP body 反向污染 Core。

代价：

- 产品必须显式配置并预算多个内存域，不能只看 `sizeof(agent_workspace_t)`；
- Provider 作者必须承担 request/response/codec/TLS 的内存文档和测试；
- build Profile 设置错误会在编译期或初始化前暴露，不会由 heap 自动“救场”；
- 跨线程 Event、持久化 Storage 和全链路零 heap 都需要产品侧额外设计。

## 验证要求

- 使用会记录 `alloc`/`free` 次数的 Runtime allocator，断言 caller-workspace 路径中的
  `agent_init()`、注册、`agent_start()`、多轮 `agent_run()`、Session 操作和 destroy 均为零
  Core allocation；
- `agent_create()` 单独测试其唯一允许的 Core convenience allocation，并验证配对释放；
- 每个 Profile 对 Core 实际布局执行编译期大小/对齐检查；不足时构建失败；
- scratch 测试覆盖多轮 mark/rewind、峰值统计、错误路径回滚和任何跨 turn 指针泄漏；
- Session Storage Provider 覆盖历史窗口、缓存耗尽、保留淘汰、损坏尾部恢复和清除；Core 覆盖
  查询预算、完整 turn 投影与 Storage I/O 错误传播；
- Event observer 测试确认 callback 不触发 Core allocation；异步 Adapter 单独验证预分配队列的
  溢出策略；
- 每个 Provider/Transport 至少标注 fixed workspace、bounded allocator 或 external heap 模式，
  并报告请求、响应、JSON、TLS 和连接状态的峰值；
- 需要全链路零 heap 的产品应在其实际 Provider/Transport/Tool 组合上执行分配计数测试，不能
  仅复用 Core 测试结论。
