# ADR 0010: Context 的编排、投影与资源边界

- 状态：提案
- 日期：2026-09-26

本文提到的 `agent_turn_end()` 是后续候选接口；同步 MVP 以 `agent_run()` 返回结束运行，见 ADR 0020。

## 背景

Agent 的“上下文”容易被误解为由 Core 长期持有的一份大字符串，或由 Model Provider
自行从 Session、Tool、Skill 和设备状态拼装的请求体。前者会使 Core 绑定长期记忆、设备
数据源和厂商协议；后者会使不同 Model Provider 得到不一致的 Agent 行为，且无法统一预算、
策略和失败语义。

cAgentV1 每次运行从 request arena 分别构造 `context`、`tools_json` 和 `messages_json` 三个
NUL 结尾缓冲区，再交给 OpenAI-compatible Model。这个做法适合首个同步产品闭环，但把
OpenAI JSON 格式带入了 Core，也让三个固定 buffer 的峰值和寿命相互耦合。

V2 面向 ESP-IDF、openvela、RT-Thread 和 Host 的同一套 Core。它需要统一管理模型可见
信息的来源、顺序、上限和生命周期，同时允许应用按产品选择设备状态、Skill、持久化记忆
或远端检索能力。

## 决定

Context 是**每个 turn 面向 Model 的有界投影**，不是一份独立的长期存储，也不是
OpenAI 请求 JSON。Core 负责该投影的编排、预算、顺序、取消和有效期；各领域模块继续
持有其原始状态。

```text
Session Storage Provider ──┐
Tool registry              ├─> Core projection ─> Model request views
Skill registry             │       (turn scratch)       |
Context providers          ┤                            v
Optional Memory provider ──┘                    Provider wire encoding
```

### 责任边界

| 信息或能力 | 原始状态所有者 | Core 的职责 | Model Provider 的职责 |
|---|---|---|---|
| system prompt / Agent 指令 | 应用配置或 Core 复制的配置 | 选择、计入预算、输出最终 `system_prompt` | 映射到目标模型请求字段 |
| Session 历史 | Session Storage Provider | 维护当前 turn 事务、按预算查询完整历史 turn group、投影 `agent_message_view_t[]` | 编码目标协议的 messages |
| Tool 定义与 schema | 应用持有或 Tool Registry 的受控副本 | 筛选可见 Tool、Policy 检查、投影 `agent_tool_view_t[]` | 编码目标协议的 tools/function schema |
| Skill | 应用提供、Registry 注册 | 按注册模式投影：INLINE 全文或 ON_DEMAND 摘要（[ADR 0028](0028-skill-projection-modes.md)） | 不解释 Skill 领域含义 |
| 动态 Context | Context Provider / 应用 | 调用、排序、限制输出、处理失败 | 不直接读取设备或应用状态 |
| 长期 Memory | 可选的外部 Memory Provider | 将选中的结果作为受限 Context 贡献 | 不保存或检索长期记忆 |
| OpenAI JSON / HTTP body | Model Provider | 不持有、不生成 | 序列化、发送、解析和释放/复用临时 buffer |

Core 可以持有注册项、当前 session 的绑定/游标和本 turn 的投影副本，但不持有完整 Session
历史，也不取得应用状态、Memory 后端或 Provider wire buffer 的所有权。所有跨边界文本都使用 `agent_string_view_t`；除非
接口明确标为 COPIED，否则 Provider 和应用不得在 callback 返回后保留 view。

### Model 输入不是单一字符串

V2 的规范化 Model 输入由三个正交部分组成：

```c
agent_model_request_t request = {
    .system_prompt = context_text,
    .messages = message_views,
    .message_count = message_count,
    .tools = tool_views,
    .tool_count = tool_count,
};
```

- `system_prompt` 是 system 指令、启用 Skill、动态 Context 和将来 Memory 检索结果的有界
  文本投影。
- `messages` 是 Session 的有序对话投影；它不是 `system_prompt` 的拼接内容。
- `tools` 是可调用 Tool 的结构化投影；它不是 system text，也不是 Core 生成的
  `tools_json`。

OpenAI-compatible Provider 可以将上述三部分编码为 `messages`、`tools` 与 HTTP JSON body；
本地模型、其他云模型或无 Tool 模型可以使用不同编码。Core 不公开也不缓存
`messages_json`、`tools_json`。

### Context Provider 契约

`agent_context_provider_t` 是动态 Context 的贡献定义：

- `build()` 在驱动 Agent 的同一任务上下文中同步执行，不由 Core 创建线程，也不允许 ISR
  调用。
- Core 按 `priority` 降序调用；相同 priority 保持注册顺序，保证可复现的模型输入。
- `agent_context_request_t`、取消 token 和 text sink 只在此次 callback 内借用有效。
- Provider 通过 `agent_text_sink_t` 分段输出。Core 将每个 chunk 复制到当前 turn 的
  Context scratch，Provider 不得保留 sink 或依赖其在返回后继续可用。
- Provider 必须轮询 cancel 和 deadline；不得在 build 中无界等待网络、锁或外设。
- 注册项中的 `name` 与 `user_data` 是借用引用。注销前应用必须确保其不再被 Core 使用。

取消和 deadline 是 turn 级控制：任一 Provider 观察到取消或 deadline 到期时，当前 turn
分别以 `AGENT_ERROR_CANCELLED` 或 `AGENT_ERROR_TIMEOUT` 终止，`required` 不改变该规则。
`required == true` 的其他失败或 Context 总预算超限同样终止 turn；非必需 Provider 的其他
失败应被记录为事件/诊断事实后跳过，具体事件字段留待 Event 契约冻结时定义。非必需贡献
不得因失败而静默改变已写入文本的边界：Core 要么丢弃该 Provider 的整段输出，要么在调用前
预留可验证的输出空间；首版实现采用“按 Provider checkpoint 回退整段输出”。

### 组装顺序和预算

首版规定如下逻辑顺序：

1. 固定 system prompt；
2. 已启用的 Skill；
3. 已注册的动态 Context Provider；
4. 将来由可选 Memory Provider 返回的检索结果；
5. Session 消息与 Tool 列表作为独立结构化投影，而非拼入 system text。

每类贡献的上限由 build Profile 的 `CONFIG_AGENT_MAX_CONTEXT_BYTES`、
`CONFIG_AGENT_SCRATCH_BYTES` 及相关容量宏确定。无论如何最终 Context 都不得超过这些固定
边界。Core 在写入前检查容量；不得以截断 JSON、截断 UTF-8 字节序列或悄悄丢弃 required
内容的方式“凑合成功”。

`AGENT_ERROR_CONTEXT_OVERFLOW` 仅表示最终 system/context 文本投影无法满足预算。Session
消息描述符、Tool schema、Model HTTP body 或 Provider 输出 buffer 的容量不足分别属于其
所属模块，不得滥用该错误码；详见 ADR 0008。

### Session 历史窗口

Session 历史由 Storage Provider 保存和按需读取；Core 不读取整段历史后再裁剪。每次模型请求
只选择最近的、已经结束的完整历史 turn group。目标 API 在 `agent_limits_t` 增加
`max_history_turns`：它是本次请求最多投影多少个**此前完整** turn group 的运行期窗口，既不是
一次执行的 `max_steps`，也不是存储后端的保留数量。`max_history_turns == 0` 表示不投影持久化
历史；默认值将由 `AGENT_LIMITS_DEFAULT` 给出。

一个 turn group 可包含 user 消息、assistant Tool call、Tool result、assistant final 或 abort
事实。Core 必须从最新 group 向前选择，并同时满足下列边界：

- `max_history_turns`；
- build Profile 的 `AGENT_MAX_PROJECTED_MESSAGES`，用于限制 scratch 中
  `agent_message_view_t[]` 的数量；
- `AGENT_MAX_CONTEXT_BYTES`、可用 turn scratch 与其他本轮必需投影的预算。

若一个较旧的完整 group 无法整体放入剩余预算，Core 跳过该 group，不得留下孤立的 Tool call 或
Tool result；最终送给 Model 的已选 group 恢复为时间正序。旧历史因窗口或预算未被投影是正常
裁剪，不是 `AGENT_ERROR_CONTEXT_OVERFLOW`。Storage 读取失败按 Provider I/O 契约返回错误，除非
未来显式增加“历史可选”的降级策略。

历史保留、JSONL/Flash/NVS 编码、PSRAM 热缓存、压缩、checkpoint 和清理策略属于 Session
Storage Provider，不属于 `agent_limits_t` 或 Core workspace。详见 ADR 0016。

### 生命周期与内存

Core workspace 在 `agent_init()` 时被切分为长期状态和可复用 turn scratch。长期状态只保存
session binding、游标和当前事务事实，不保存跨 turn 的完整消息/event payload。Context 文本、
message view 数组及 Tool view 数组的有效期至少覆盖一次 Model `complete()` 调用。模型返回
后，只有后续状态机仍需使用的结果，例如待确认的 Tool call，才必须保留到该 turn 结束或
中止；不再使用的输入投影可以在实现内部复用其 scratch。

一个 Agent 首版仅允许一个 active turn，因此 scratch 可由该 turn 独占。`agent_turn_end()`
或终止清理后，Core 回退 scratch mark，后续 turn 复用同一片内存。Core 主链路不在每次
turn 创建 heap arena，也不执行无界增长。

OpenAI Provider 的 request/response JSON、cJSON DOM、TLS/HTTP buffer 和网络任务栈不属于
编译期 `agent_workspace_t`；产品 Profile 必须在独立内存预算中声明它们，参见 ADR 0006 与
ADR 0007。Provider 不得把 Core scratch 作为其私有长期 allocator。

### Memory 的位置

长期 Memory 在 V2 首版不实现为 Core 内建模块。Flash/NVS、文件、网络服务、向量检索和
隐私策略的差异过大，过早固定 `memory.h` 的行为会制造无实现契约。

后续若引入 Memory Provider，它必须通过受限检索结果或 Context Provider 等价接口贡献文本，
并满足同样的 cancellation、deadline、排序、预算和借用期规则。Memory Provider 不得绕过
Context Builder 直接修改 Session、system prompt 或 Model Provider 请求。

## 不采用的方案

### Core 持有一个可随时修改的全局 Context 字符串

无法区分 Session 历史、静态指令、设备状态和长期记忆的所有权；并发或多 Session 时内容会
泄漏，且更新后的预算和回滚语义不清晰；拒绝。

### Core 预先构造 `tools_json` 与 `messages_json`

把 OpenAI-compatible wire format 固化为通用 Agent ABI；本地模型和其他云 Provider 必须适配
不属于它们的中间格式，并增加不必要的固定 JSON buffer；拒绝。

### Model Provider 直接读取 Session、Tool 与应用状态

Provider 将绕过 Policy、Context 预算和统一的 Session 行为；不同 Provider 可能得到不同
上下文，且 Provider 需要依赖 Core 私有结构；拒绝。

### 将长期 Memory 作为首版 Core 组件

持久化、一致性、检索、加密、生命周期和资源上限尚无已验证契约。首版只保留 Context
贡献边界，待真实产品需求确认后再设计 Provider 契约；拒绝。

## 影响

正面影响：

- Core 对不同模型保持厂商协议无关，同时统一控制模型可见信息；
- Session、Tool、Skill、动态设备状态和未来 Memory 的所有权清晰；
- Context 的字节预算、取消、错误与 scratch 生命周期可以在 MCU 上预测和测试；
- OpenAI Provider 可以独立选择 cJSON、流式协议和 HTTP Adapter，而不污染 Core API。

代价：

- Provider 必须实现从规范化 view 到目标 wire format 的序列化；
- 应用编写动态 Context 时必须显式处理输出大小、deadline 和数据有效期；
- 需要在实现阶段定义 Context checkpoint、非必需失败事件以及每个 build Profile 的精确容量。

## 评审重点

1. 固定 system prompt、Skill、动态 Context 与未来 Memory 的默认顺序是否符合首批智能家居
   与小智类产品；当前结论是“符合”，但应允许配置关闭各类贡献。
2. 非必需 Provider 失败时是否应由首版 Event API 暴露诊断事件；当前结论是“应暴露”，但
   事件枚举和 payload 尚未冻结。
3. `required` 是否足以表达 Context 的降级策略，还是需要未来增加优先级预算、类别或
   reserve 字段；当前结论是“首版足够，不预设复杂配额”。
4. Memory Provider 是直接扩展 `context.h`，还是未来单独定义 `memory.h` 后适配为 Context
   贡献；当前倾向后者，以保留持久化与检索的领域边界。

## 验证要求

- Context Provider 的顺序在相同配置下可复现；
- required/non-required Provider 的成功、失败、取消、超时和超限路径都不泄漏 scratch；
- 单个 Provider 部分写入后失败时，checkpoint 回退不留下半段 Context；
- Session、Tool、Context 与 Model request 的所有 view 在 Provider `complete()` 返回前有效；
  Provider 返回后不得保留或使用它们；
- 无 Model、无 Tool、无 Context Provider、空 Session 与空 system prompt 都有定义行为；
- 不同 Model Provider 对同一规范化投影能独立编码，Core 不依赖 OpenAI JSON 字段；
- 在目标 Profile 测量 Core workspace、Provider JSON、TLS/HTTP 和任务栈的独立峰值。
