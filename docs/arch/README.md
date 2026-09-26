# cAgentV2 模块责任地图

`docs/architecture.md` 是当前唯一完整的架构事实来源。本目录中的各模块文档尚未
创建；以下表格是后续拆分计划，不是可访问文档索引。模块文档开始编写后，必须先从
总体架构中迁移相应契约，再将链接加入本页。

## 模块分类

| 分类 | 模块 | 计划文档 |
|------|------|------|
| Core Kernel | Lifecycle、Workspace、Registry、Event Dispatch、Cancel/Stats | `core.md` |
| Execution / Orchestration | Loop、Run State Machine | `loop.md` |
| Execution / Orchestration | Context Projection | `context.md` |
| Capabilities | Model | `model.md` |
| Capabilities | Tool | `tool.md` |
| Capabilities | Skill | `skill.md` |
| Capabilities | Session | `session.md` |
| Capabilities | Memory | `memory.md` |
| Cross-cutting Control | Policy、Confirmation、Validation | `policy.md` |
| Cross-cutting Control | Live Event Dispatch | `event.md` |
| Platform Services | Runtime、Transport | `runtime.md` |
| Platform Services | Session Storage | `storage.md` |
| Composition | Plugin、Profile | `plugin.md` |
| 外围（非 Core） | Trigger、Scheduler | `trigger.md` |

## 已收敛的边界决定

以下决定已在总体架构中收敛，后续模块文档只引用：

1. 无 Core Gateway 门面，主链路为类型化直接调用（architecture.md §5.6）。
2. 无通用 Hook Registry；控制点类型化，观测走 live event（§20.2）。
3. Permission 不独立成子系统，是 Policy chain 的 contributor（§14.2）。
4. Tool 使用受限类型化 descriptor 注册；内部 JSON codec 负责产生模型可见 schema，
   不实现完整 JSON Schema 验证（ADR 0006）。
5. Context 投影策略由 Kernel 固定，仅 Context Provider 可插拔（§14.3）。
6. Kernel 拥有运行状态机，Loop 只贡献 step 决策（§10.2）。
7. Trigger/Scheduler 属于 Application 外围，Core 不提供 Trigger Registry（§4）。

## 待收敛的跨模块决定

1. Model/Transport 的 I/O 模型：同步 `complete/request` 是最小实现路径；是否升级为
   `start/poll/cancel` 的非阻塞契约，必须在公开 `model.h` 与 `transport.h` 前决定。
2. Port capability 的最小集合：Runtime 只抽象 allocator、时钟、同步/临界区与日志；
   网络、TLS、文件系统和硬件服务由独立 provider 或应用适配层提供。
   HTTP Transport 的候选 Adapter 边界、构建裁剪和同步语义见
   [ADR 0007](../adr/0007-http-transport-adapters.md)，在接受前仍可调整。

## 阅读顺序

建议先阅读 [总体架构](../architecture.md)，再按 `core` -> `loop` -> `model` -> `tool`
-> `policy` -> `session` -> `context` 的顺序创建模块文档。
