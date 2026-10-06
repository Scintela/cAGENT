# cAgentV2 模块责任地图

[总体架构](../architecture.md) 保留目标设计；已有模块文档说明实际实现边界。
以下表格中只有链接项表示已创建文档，其余仍为拆分计划。

## 模块分类

| 分类 | 模块 | 计划文档 |
|------|------|------|
| Core Kernel | Lifecycle、Workspace、Registry、Event Dispatch、Cancel/Stats | `core.md` |
| Execution / Orchestration | Loop、Run State Machine | `loop.md` |
| Execution / Orchestration | Context Projection | [Context 架构](context.md)，[公共接口](../api/context.md) |
| Capabilities | Model wrapper (`src/model/`) and optional implementations (`providers/`) | `model.md` |
| Capabilities | Tool | [Tool 架构](tool.md)，[公共接口](../api/tool.md) |
| Capabilities | Skill | [Skill 架构](skill.md)，[公共接口](../api/skill.md) |
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
3. Permission 不独立成子系统，由应用在单个产品 Policy 回调中组合（§14.2）。
4. Tool 注册借用的 schema JSON 对象，reader 做语法/唯一键准入，不执行完整 JSON Schema；
   模型 wire JSON 由 Provider 生成，无 schema 缓存（Tool 架构、ADR 0022）。
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
