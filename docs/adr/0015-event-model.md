# ADR 0015: Live Event 观测模型——边界事件、单回调与双通道

- 状态：提案
- 日期：2026-09-26

## 背景

cAGENT V1 提供单一事件回调（11 种事件类型，含 `ERROR`、`CANCELLED`、`TIMEOUT`
独立类型；扁平 NUL 字符串载荷 + `iteration` 序号），回调在 `agent_run()` 线程同步
执行、纯通知不干预控制流。该模型可用，但存在三类问题：事件类型混入控制流分支，
应用需 switch 全部类型才能拼出终态；载荷不含执行事实，观测者须自行累计；观测事实
（消息、工具结果）与通知（状态变化）共用一条通道，无法区分"可回放审计"与"运行
期旁观"。

V2 草案 `include/agent/event.h` 收敛为 7 种事件、载荷嵌入工具调用事实与执行摘要。
architecture.md §20 确立 durable session event 与 live runtime event 双通道，但
§20.2 仍列出 `TIMEOUT`、`CANCEL_REQUESTED`、`RESOURCE_PRESSURE`、`ERROR` 等与
当前头文件不一致的事件名——本文一并裁决该漂移。

本 ADR 参考 OpenAI Agents SDK hooks（run/agent 两层作用域）、LangChain callbacks
与 LangGraph `astream_events`（push 观测 / pull 消费分离）、OpenTelemetry GenAI
semantic conventions（`invoke_agent`/`chat`/`execute_tool` span 与 opt-in payload）
以及 openvela ai_agent（run_id 日志追踪 + message_bus 路由）的公开设计，固化 V2
live event 模型并给出增补建议。

## 决定

### 1. 事件种类编码观测边界，不编码控制流分支

Live event 只在**可观测边界**发出，共 7 种：

| 事件 | 边界 | OTel GenAI 映射 |
|------|------|-----------------|
| `AGENT_EVENT_TURN_BEGIN` | 请求准入 | `invoke_agent` span 开始 |
| `AGENT_EVENT_MODEL_BEGIN` / `MODEL_END` | Provider 调用前后（含失败） | `chat` span |
| `AGENT_EVENT_TOOL_BEGIN` / `TOOL_END` | handler 调用前后（含失败） | `execute_tool` span |
| `AGENT_EVENT_CONFIRMATION` | turn 暂停等待授权 | agent span 事件 |
| `AGENT_EVENT_TURN_END` | 终局，含 abort，恰好一次 | `invoke_agent` span 结束 |

失败、取消、超时不是独立事件类型，而是对应边界事件的 `status` 载荷；终态原因统一
由 `TURN_END.status` 携带。architecture.md §20.2 中 `TIMEOUT`、`CANCEL_REQUESTED`、
`RESOURCE_PRESSURE`、`ERROR` 等独立事件名**取消**；如评审认为取消请求需要独立于
终局的即时可见性，按评审重点 3 复议为 `TURN_END` 之外的附加载荷而非新类型。

### 2. 载荷自带执行事实

`agent_event_t` 嵌入 `agent_run_summary_t`（模型调用数、工具成功/失败/拒绝计数、
耗时、scratch 峰值）与 `agent_tool_call_view_t`（工具/确认事件的完整调用事实，
含参数 JSON 视图）。观测者在任意边界读到当前事实，无需自行累计或回查内核。

Live 事件不携带 payload 正文（消息内容、工具输出文本）。正文采集属于 durable
session event 通道，按 §11.2 的 NONE/METADATA/FULL 分级控制——与 OTel GenAI
"payload 显式 opt-in"一致。

### 3. 单回调、同步、驱动线程、不可重入

- 每个 Agent 至多一个回调，`agent_set_event_callback()` 替换式设置，仅
  CONFIGURING/READY，运行期返回 `AGENT_ERROR_BUSY`；
- 回调在驱动 turn 的任务中同步执行，必须快速返回；重操作入队到应用自己的 worker；
- 回调内不得重入 Agent（包括 `agent_cancel`）、不得修改注册表；
- 不提供回调列表/注册表、不提供独立事件线程或异步事件队列。

### 4. 观察者与拦截者分离

Event 回调只读不控；授权决策走 policy 回调（`policy.h`），确认流走
`agent_turn_resume()`。观测回调的任何返回值都不改变控制流。

### 5. 双通道：live event ≠ durable session event

Live event 是瞬态观测（回调返回即失效，不进任何日志）；durable session event 是
有界 append-only 事实流（进 session log，支持回放、审计、恢复）。两者事件集合、
生命周期、消费方式独立，禁止合并。

### 6. push 观测与 pull 消费分工

Live event callback 是 **push 观测轨**（tracing、watchdog、审计 side effect）；
可驱动状态机的 `agent_turn_step()` + `agent_step_result_t` 是 **pull 消费轨**
（流式 UI、应用状态机推进）。应用不得在 event 回调中承担 UI/业务消费职责。
该分工对应 LangGraph"callbacks 用于观测、event stream 用于应用消费"的经验教训，
须写入 README/文档。

### 7. 建议增补（待评审确认后并入头文件）

| 增补 | 内容 | 动机 |
|------|------|------|
| 事件单调序号 | `agent_event_t` 增加 `uint32_t seq`，Agent 级单调递增 | 观测输出经慢速通道（UART/日志/网络 exporter）转发后乱序排查困难；V1 的 `iteration` 曾承担此锚点，V2 事件无序号 |
| trace_id 自动派生 | `TURN_BEGIN` 时 trace_id 为空则库生成单调短 ID | ai_agent 的 run_id 证明关联 ID 对排查体验的价值；不传 trace_id 的用户也应获得全程关联 |
| OTel 映射示例 | examples 层提供 live event → OTel GenAI 属性（span 类型、`gen_ai.usage.*`、latency）的导出示例 | 开源定位的差异化卖点；仅示例层，不动内核 |
| 文档分工页 | push/pull 双轨、回调禁令、借用期表 | 观测契约目前散在头文件注释 |

## 不采用的方案

- **回调列表 / 每类事件独立回调**：需要动态数组或固定位图注册表，观测者数量与
  调用扇出不可预算；单回调 + 应用扇出保持有界；拒绝。
- **`ERROR`/`CANCELLED`/`TIMEOUT` 独立事件类型（V1 形态）**：枚举编码控制流分支，
  类型集合随错误分类膨胀，应用须拼装终态；终态收敛进 `TURN_END.status` 后类型
  集合最小且信息不减；拒绝（架构 §20.2 同步删除）。
- **事件总线（ai_agent message_bus 形态）**：message_bus 是 channel 间 IO 路由
  （feishu/websocket/cli ↔ agent loop），属应用外围；观测事件与消息路由是两类
  问题，合并会把 channel 概念拉进内核，违反架构 §7；拒绝。
- **异步事件队列 / 独立事件线程**：引入隐藏任务、锁与背压问题，破坏单写者模型
  （§19）；慢消费者由应用侧 worker 承担；拒绝。
- **模型 token 级流式事件**：live 事件粒度止于"模型调用边界"；token 流属
  provider response sink 层（`agent_model_sink_t`），混入事件回调会让回调时长
  不可控；拒绝。
- **run/agent 两层作用域回调（OpenAI Agents SDK 形态）**：V2 明确单 Agent 单
  turn 为非多 Agent 编排目标；引入分层只有当子 Agent 进入路线图时重开评审。

## 影响

正面影响：

- 事件类型集合最小（7 种），终态与原因经 `status` 表达，应用接入成本低；
- 每个边界自带执行事实，观测者无需回查内核即可绘制进度；
- 观测与拦截分离，观察回调不可能意外改变控制流；
- live/durable 双通道边界清晰，payload 采集分级与 OTel 原则一致；
- 与 OTel GenAI span 模型天然对齐，示例 exporter 即可接入标准观测生态。

代价：

- 事件载荷增大（嵌入 summary 与 tool call 事实），栈上构造事件的开销高于 V1
  扁平结构；
- 运行期替换回调被禁止（CONFIGURING/READY 限定），需要动态开关观测的产品须在
  回调内部自行短路；
- 取消请求需等到 `TURN_END` 才作为事件事实出现，无法在事件流中即时观测"已请求
  取消"（若评审认为必要，见评审重点 3）。

## 评审重点

1. 事件序号 `seq` 的作用域：Agent 级单调（跨 turn 连续）还是 turn 级；倾向
   Agent 级，与 `confirmation_id` 是否共用同一计数器需定。
2. trace_id 自动派生的形态：单调十六进制短 ID（ai_agent run_id 风格）还是
   时间戳派生；是否允许应用后续以同名 trace_id 覆盖。
3. 取消请求的可见性：`agent_cancel()` 是否需要发出独立轻量事件或附加载荷，
   还是维持"终局统一在 `TURN_END`"。
4. `MODEL_END`/`TOOL_END` 的 `status` 是否足以区分"provider 失败/超时/取消"，
   还是需要在 summary 中补充细分计数。
5. OTel 映射示例的落点：`examples/`（纯示例）还是 `plugins/`（可复用导出器）。
6. architecture.md §20.2 的旧事件名清单删除后，是否保留 `RESOURCE_PRESSURE`
   作为未来 scratch 高水位事件的预留名。

## 验证要求

- `include/agent/event.h` 可独立以 C99 与 C++11 编译；
- 事件序列测试：完整 turn 的 `TURN_BEGIN → (MODEL_BEGIN/END → TOOL_BEGIN/END)*
  → TURN_END` 顺序、`TURN_END` 恰好一次、失败/取消/超时路径均以 `status` 收敛；
- `seq` 单调递增且跨 turn 连续（若采纳评审重点 1 的 Agent 级方案）；
- 回调禁令测试：回调内调用 `agent_run`/`agent_cancel`/注册接口返回错误或断言；
- 视图有效期测试：事件内视图在回调返回后失效（ASAN 验证不保留指针）；
- summary 增量一致性：事件内 summary 与 `agent_get_stats()` 累计值在 `TURN_END`
  处吻合；
- OTel 映射示例覆盖三种 span 与 `gen_ai.usage.*` 属性的字段对照表。
