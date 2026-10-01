# ADR 0016: Session 历史投影与 Storage Provider

- 状态：提案
- 日期：2026-09-27

## 背景

嵌入式 Agent 的 Session 会跨多个 turn 持续增长。完整历史可能需要以 JSONL 写入文件/Flash、
以 NVS checkpoint 或分块记录保存，并在资源较充足的设备上以 PSRAM 维持热缓存。把 event ring
和 payload pool 固定放进 `agent_workspace_t` 看似简单，但它把历史长度与 Core RAM 绑定，无法
支持掉电恢复、较长会话、外部存储或产品各自的保留规则。

另一方面，Core 不能把历史完全交给 Model Provider：user/assistant/Tool call/Tool result 的
完整性、取消和失败事实仍是 Agent 的执行不变量。也不能每轮加载全部历史后随意截断，因为这会
耗尽 MCU RAM，并可能拆开 Tool call/result 配对。

## 决定

### Session 历史由 Storage Provider 所有

Session Storage Provider 是独立、可替换的能力。它拥有历史记录、编码、持久化介质、缓存、索引、
checkpoint、恢复和保留策略；Core 不把完整历史、event descriptor ring 或历史 payload 常驻在
`agent_workspace_t`。

```text
Core workspace (typically SRAM)
|-- Agent lifecycle and registries
|-- active turn, session binding/cursor and transaction facts
`-- reusable turn scratch

Session Storage Provider workspace (often PSRAM)
|-- metadata/index and hot-history cache
|-- JSONL/Flash/NVS read-write staging
`-- checkpoint/compaction state

Persistent medium
`-- filesystem JSONL, Flash journal, or NVS chunks/checkpoints
```

Flash/NVS/filesystem 是持久化介质而非 caller workspace。JSONL 适合文件系统或 append-only flash
日志；NVS 是键值存储，Provider 应采用分块记录、序号和 checkpoint，而不是把无界 JSONL 文本塞进
单个 value。

Core 仍拥有下列执行语义：

- 以完整 turn group 记录 user 消息、assistant Tool call、Tool result、assistant final 或 abort；
- 不允许孤立 Tool result、重复 call ID result，或将不完整 group 当作稳定历史；
- 在一次 turn 内维护 session binding、cursor、取消、deadline 和写入事务；
- 按本 ADR 的窗口与预算请求历史，并把规范化 view 交给 Model Provider。

Storage Provider 不得绕开这些规则，把任意原始记录直接投影成 Model messages。

### 历史读取是有界投影，不是完整回放

`agent_limits_t` 应增加下列运行期行为字段：

```c
uint32_t max_history_turns; /* Previous complete turn groups projected per request. */
```

其语义如下：

- 只计算当前请求之前已经结束的完整 turn group；当前 user 输入不是历史 group；
- `max_history_turns == 0` 禁用持久化历史投影，不表示无限制；
- 非零值是上限，不保证一定取得这么多 group；默认值由 `AGENT_LIMITS_DEFAULT` 定义；
- 它不控制执行步骤（`max_steps`）、存储保留数量、PSRAM 缓存大小或 Flash/NVS 占用。

Core 从最新 group 向前查询和准入，随后按时间正序写入 `agent_message_view_t[]`。每个候选 group
只有在整体满足全部限制时才可加入：

1. `max_history_turns`；
2. build Profile 的 `AGENT_MAX_PROJECTED_MESSAGES`；
3. Context 字节预算、剩余 turn scratch 和本轮其他必需对象的峰值预算。

若一个较旧 group 不能整体加入，Core 跳过它；不得仅留下其中的 Tool call、Tool result 或半段
assistant 文本。因窗口或预算未选入旧历史是正常裁剪，不是 `AGENT_ERROR_CONTEXT_OVERFLOW`。Storage
读取/解码错误按 Provider 的 I/O/parse 错误上报，首版不静默退化为“没有历史”。

`AGENT_MAX_PROJECTED_MESSAGES` 是 build Profile 的硬上限，而不是运行期 `max_history_turns` 的重复。
一个 turn group 可含多条消息与多次 Tool 调用；只有前者才能约束 Core scratch 中 view 数组的最大
长度。它应在实现 Session 投影前加入配置头和 Profile 生成器。

### 保留、缓存和恢复由 Provider 配置

以下项目不进入 `agent_limits_t`、`agent_config_t` 或 Core workspace：

| 项目 | 所有者 | 示例 |
|---|---|---|
| 历史保留 | Storage Provider | 最大日志字节数、最大保存天数、最大历史 turn、按 session 删除策略 |
| 热缓存 | Storage Provider | PSRAM cache bytes、缓存的 session/turn 数、淘汰策略 |
| I/O staging | Storage Provider | JSONL 读写块、NVS chunk、Flash page buffer |
| 恢复和压缩 | Storage Provider | checkpoint 间隔、compaction、水位线、损坏尾部处理 |
| 介质安全 | Storage Provider/产品 | 加密、密钥、擦除、wear leveling、隐私策略 |

掉电时，Provider 必须能识别不完整写入；恢复后忽略或回滚损坏尾部，保留此前完成的 group。对于
已发生的副作用 Tool，abort/未知完成状态必须作为事实记录，不能因日志恢复而伪装为动作未发生。

### Core 与 Provider 的内存和生命周期边界

Provider 的历史 view 只在其定义的查询 callback 或 Core 当前投影阶段内借用有效。Core 需要在
调用 Model 前把必要的 message descriptor 与文本复制或固定到 turn scratch；Provider 不得保留
Core scratch 指针。turn 结束时 Core 复位 scratch，Provider 的缓存与持久化数据不受影响。

应用分别放置和预算：Core workspace 通常位于内部 SRAM；大历史缓存、JSON/HTTP buffer 可以位于
PSRAM；Flash/NVS/文件系统容量和写入寿命另行计算。一个 Provider 可使用固定 workspace、受限
allocator 或明确声明的 heap，但不得把其分配行为伪装为 Core 的零 heap 承诺。

## 不采用的方案

### 在 Core workspace 固定保存整个 Session

历史长度、掉电恢复和缓存策略会强制绑定到每个 Agent 的 SRAM Profile；大历史会挤占 registry 与
turn scratch，且不能自然支持 Flash/NVS/PSRAM；拒绝。

### 每轮读取完整历史后再截断

读取时间、RAM 峰值和解析工作随历史无界增长，并容易在字节截断时破坏 Tool call/result 关系；拒绝。

### 以 `max_history_turns` 控制存储保留

投影窗口是一次模型请求的行为选择，保留策略是数据生命周期/隐私/I/O 策略。二者的责任方和变更
频率不同；拒绝。

### 将 JSONL/NVS 格式固定到 Core API

文件系统、raw flash、NVS 的原子性、容量和磨损模型不同。Core 只定义规范化记录与投影语义；具体
编码属于 Provider；拒绝。

## 影响

正面影响：

- 长 Session、掉电恢复与 PSRAM 热缓存不再扩大 Core workspace；
- Core 可以保持零 heap 主路径，同时按固定 scratch 预算构造模型上下文；
- 不同平台可采用 JSONL、Flash journal 或 NVS，而不改变 Tool/turn 语义；
- 历史投影窗口、消息描述符上限和存储保留不再混用同一个 `max_turn` 概念。

代价：

- Session Storage Provider 需要定义查询、追加、提交、恢复、错误和并发契约；
- 产品必须单独预算 PSRAM/Flash/NVS 和 I/O 延迟，不能只观察 `sizeof(agent_workspace_t)`；
- 当前 `MAX_SESSIONS`、`SESSION_EVENT_CAPACITY`、`SESSION_PAYLOAD_BYTES` 的 RAM-only 草案需要
  在 Provider API 落地时迁移，不能同时被视为 Core 与 Provider 的容量。

## 验证要求

- Storage Provider 可在损坏尾部、掉电后恢复此前完整 turn，拒绝不完整 group；
- 历史投影不会拆开 Tool call/result，且永远不超过 `max_history_turns`、
  `AGENT_MAX_PROJECTED_MESSAGES` 与 scratch 预算；
- 空历史、禁用历史、历史读取 I/O/parse 失败、缓存满和保留淘汰都有定义且可测试的行为；
- 多轮运行后 Core allocation 计数仍为零，历史 cache/I/O 分配只归 Provider 统计；
- JSONL、Flash journal 和 NVS Provider 对同一规范化完整 turn 记录产生一致的投影语义；
- 断电恢复后，带副作用 Tool 的 abort/未知状态不会被伪装为未执行。

当前可选的 JSONL 后端采用“一行一个已结束 turn”，仅满足已提交历史的有界恢复；
执行中的 Tool 意图不会在动作前落盘，因此**尚不满足**上一条对副作用追溯的要求。
需要该保证的产品不得仅凭此后端宣称掉电安全，应增加预写日志或等价事务协议。

## 迁移说明

本 ADR 优先于 ADR 0010、0011、0014、公共 API 文档和总体架构中关于“Session event/payload
pool 位于 Core workspace”或“完整历史由 Session Manager RAM 持有”的旧草案。它不表示当前
`session.h` 已经提供稳定的 Storage Provider API；接口应在查询、事务、恢复和错误语义经测试后再
公开。
