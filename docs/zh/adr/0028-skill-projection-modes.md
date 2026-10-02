# ADR 0028: Skill 投影双模式（注册期声明）

- 状态：已采纳（契约设计）；Skill registry 实现与读取 Tool 待完成
- 日期：2026-10-01
- 关联：[ADR 0010](0010-context-projection.md)、[ADR 0020](0020-synchronous-run-mvp.md)、[ADR 0023](0023-file-backed-memory-soul-skill.md)、[ADR 0025](0025-builtin-tools-boundary.md)

## 背景

`AGENT_MAX_CONTEXT_BYTES`（默认 4096）是 system 指令、Skill 与动态 Context 的共享预算。
全文静态投影无法扩展到真实规模的 Skill 库：少数 KB 级 Skill 即占满预算，被裁剪的
Skill 对模型完全不可见。另一方面，摘要 + 按需读取的渐进式方案已被云端强模型实践
验证，但嵌入式产品常用弱模型或本地模型，工具调用纪律没有保障，且每次按需读取消耗
一次 Tool 调用与一步迭代（默认预算 4 次 / 8 步）。

ReAct 循环每步重发完整 system prompt，两种方式的每轮成本不同。设全文为 `B`、摘要为
`S`（约 40–80 字节）、共 `N` 步、Skill 在第 `k` 步被读取：

| 场景 | 全文静态 | 摘要 + 按需读取 |
|---|---|---|
| 本轮未使用 | `N × B` | `N × S` |
| 第 `k` 步读取 | `N × B` | `N × S` + 一次额外模型调用 + `(N−k) × B`（读入后进入同轮历史） |
| 每轮开头即需要 | `N × B` | ≈ `N × S + N × B` + 每轮一次额外调用，净亏 |

渐进式的收益集中在"低频使用的大 Skill"，对"高频且早用"的 Skill 是净亏损。
`max_history_turns` 默认 0 时跨轮无历史，高频 Skill 每轮重读使该亏损成为常态。

ADR 0025 已把 `read_skill` 列为条件可选 Tool，并将"Skill 是否需要按标识读取接口"
挂为待定问题。本文裁决 Skill 进入 Context 的投影模式契约；不实现读取 Tool 本身，
不改变 ADR 0025 的 Tool 边界。

## 决定

1. **库同时保留两种投影机制，选择权交给集成方**，不替产品做单一裁决。
2. **模式为 per-Skill、注册期声明**，不提供运行时切换 API。需要按轮次动态组合
   Skill 内容的应用，使用 `agent_context_provider_t` 自行实现。
3. API 使用互斥 **enum 而非 flag 位**（flag 允许无意义的组合值）：

```c
typedef enum {
    AGENT_SKILL_PROJECTION_INLINE = 0, /* 默认：全文进入每轮 system prompt 投影。 */
    AGENT_SKILL_PROJECTION_ON_DEMAND   /* 仅摘要进入能力目录；全文由模型经读取 Tool 按需获取。 */
} agent_skill_projection_t;
```

`agent_skill_t` 增加 `projection` 字段。`INLINE` 取 0：零初始化与既有 designated
initializer 保持现状语义，注册行为向后兼容。

默认值取 `INLINE` 的理由：行为完全确定、不依赖模型工具调用纪律、不消耗 Tool 与步
预算；Skill 库规模超出共享预算的产品，自行将低频大 Skill 声明为 `ON_DEMAND`。

## 契约细节

| 规则 | 语义 |
|---|---|
| `ON_DEMAND` 的 `description` | 必填。注册期为空返回 `AGENT_ERROR_INVALID`；它是模型检索该 Skill 的唯一线索。 |
| `required` 字段 | 仅对 `INLINE` 有意义（必进却放不下即配置错误，快速失败）。`ON_DEMAND` + `required` 注册期拒绝。 |
| 投影顺序 | 遵循 ADR 0023 的既定顺序（基础指令、Soul、Skill、Memory 结果）；本文细化 Skill 槽位内部为**摘要层在前、INLINE 全文在后**。 |
| 摘要层预算 | 优先于 INLINE 全文分配，尽力全量保障。 |
| 摘要层溢出 | 返回 `AGENT_ERROR_CAPACITY` 快速失败。静默裁剪会让能力不可见且无任何报错；摘要层放不下说明 Skill 数量已超出产品规模。 |
| INLINE 层溢出 | 维持 ADR 0010 语义：非 `required` 按优先级跳过，`required` 溢出失败。 |
| 摘要层生成 | 由 Core 生成，条目为 `name + description`，并附指向读取 Tool 的固定引导语，保证跨产品的模型行为一致。`name` 即读取标识（注册期已保证唯一），不新增 id 字段。 |
| 悬空组合 | Core 不校验"注册了 `ON_DEMAND` 却没有读取 Tool"——Core 无法可靠识别哪个注册 Tool 承担该职责。后果显式可诊断：模型调用不存在的 Tool 得到 `AGENT_ERROR_NOT_FOUND`。 |
| 公共只读查询 | 为核心外读取 Tool 与官方加载器（ADR 0029）提供单一真相，杜绝影子注册表：按名取内容的 const 查询（签名随实现定稿）。语义：driver task、**ACTIVE 期间同样可读**——registry 在 ACTIVE 状态不可被变更（生命周期状态机保证），视图借用期至注销；非 ISR-safe。 |

## 与其他 ADR 的关系

- ADR 0010 责任表中 Skill 一行的"按启用状态与优先级选择，写入 Context"由本文
  细化为双模式语义。
- ADR 0025 待定问题第一条（按标识读取接口）由本文回答：以注册 `name` 为标识，
  复用注册期借用的 `content`；读取 Tool 本身仍是应用注册或未来可选实现。
- ADR 0023 的文件型 Skill 与模式正交：来源（注册数据 / 文件加载）与投影方式互不
  约束。
- 不改变 Tool 契约、policy、limits 与事件；读取 Tool 走普通 `agent_tool_t` 注册
  流程，不获得任何特权。

## 分阶段落地

1. 契约先行：`agent_skill_t` 增加 `projection` enum 与上述注册校验（小改动，可先落）。
2. Skill registry 实现直接按双模式与分层预算编写，不做"先单模式再改造"。
3. 读取 Tool 维持 ADR 0025 的节奏：首个真实产品验证后再提炼为可选实现，不提前
   进入 Core 公共 API。

## 验证要求

- 两种模式与混合注册的投影内容、顺序与预算分配有单元测试。
- `ON_DEMAND` 空 `description`、`ON_DEMAND` + `required` 的注册被拒绝。
- 摘要层溢出快速失败；INLINE 裁剪顺序符合优先级与注册序。
- 悬空组合经 mock provider 脚本化验证：模型收到 `AGENT_ERROR_NOT_FOUND`。
- 读取链路的输出上限行为有界（见待定问题，落地前必须裁决）。

## 待定问题

- 读取 Tool 的输出上限与分块协议：`AGENT_MAX_TOOL_OUTPUT_BYTES`（默认 1024）小于
  典型 Skill 全文，需要 `offset + max` 分块读取或显式截断标记。
- 摘要层引导语与条目格式的 prompt 工程定稿。
- Skill 工具结果的历史投影策略：同轮内自然复看；跨轮与 `max_history_turns` 的
  交互（默认 0 意味着高频 Skill 每轮重读，代价由产品评估）。
