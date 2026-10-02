# ADR 索引

本节保存 cAgentV2 的架构决策记录（Architecture Decision Records）。每篇 ADR
记录一个边界或契约的裁决：背景、候选方案、决定与后果。接口收敛以
[公共 API 草案](../api/public-api.md) 及头文件快照说明为准；ADR 中描述的目标
能力不等于已实现能力。

## 文档原则

- 先定义边界、所有权和失败语义，再实现模块。
- 公共 API 草案在实现前必须完成使用示例和生命周期检查。
- V1 作为行为参考和迁移来源，不直接复制未经测试的实现。
- 文档中的接口在进入公共头文件前仍可调整；稳定后需要遵守 ABI 兼容策略。

## 索引

| ADR | 主题 |
|------|------|
| [0006 JSON 集成](0006-json-integration.md) | 历史 cJSON 默认方案；已由 ADR 0022 取代 |
| [0007 HTTP Transport 适配](0007-http-transport-adapters.md) | HTTP Transport 的平台 Adapter、裁剪、同步语义和所有权提案 |
| [0008 错误契约](0008-error-contract.md) | 跨平台错误类别、来源、远端失败、Tool 副作用事实与传播规则提案 |
| [0009 类型边界](0009-types-boundary.md) | 公共 `types.h` 的跨模块类型边界、Model 类型迁移与 include 规则提案 |
| [0010 Context 投影](0010-context-projection.md) | Context 的编排、模型投影、资源预算、Memory 边界与 Provider 契约提案 |
| [0011 配置边界](0011-configuration-boundaries.md) | 编译期容量 Profile、运行期 limits、workspace 与可替换 Model Provider 的边界提案 |
| [0012 agent.h 边界](0012-agent-header-boundary.md) | `agent.h` 的应用入口、聚合范围、生命周期与同步运行边界提案 |
| [0013 文本表示](0013-text-representation.md) | 公共 API 文本表示候选方案：NUL 字符串、长度视图、混合边界、双轨 API 与 `_Generic` |
| [0014 内存域](0014-memory-domains.md) | Core workspace、Session、turn scratch、Provider/Transport 与外部库的内存域和分配边界提案 |
| [0015 事件模型](0015-event-model.md) | Live Event 观测模型：边界事件集合、单回调契约、push/pull 分工、双通道与 OTel 映射提案 |
| [0016 会话历史存储](0016-session-history-storage.md) | Session 完整 turn 历史的投影窗口、Storage Provider 所有权、JSONL/Flash/NVS 持久化与 PSRAM 缓存边界提案 |
| [0017 Runtime 可移植性](0017-runtime-portability.md) | Runtime 最小平台服务、时钟/同步/allocator 契约和跨系统 Port 组织方案提案 |
| [0018 Transport 可移植性](0018-transport-portability.md) | HTTP/TLS Transport 通用契约、平台 Adapter、构建裁剪与 Provider 分层方案提案 |
| [0019 Kconfig 集成](0019-kconfig-integration.md) | Kconfig 集成边界：Core Profile 菜单、Port 包裁剪项、命名规范与等价通道提案 |
| [0020 同步运行 MVP](0020-synchronous-run-mvp.md) | 同步运行 MVP 的公开边界、取消和确认失败关闭规则 |
| [0021 平台 cJSON 复用](0021-platform-cjson-reuse.md) | 平台 cJSON 复用备选方案，未作为当前默认实现 |
| [0022 有界 JSON codec](0022-bounded-json-codec.md) | 私有 jsmn codec 的构建、内存与验证边界 |
| [0023 文件型 Memory、Soul 与 Skill](0023-file-backed-memory-soul-skill.md) | Markdown 来源的归属、显式注册、文件 I/O 解耦与有界 Context 投影提案 |
| [0024 Session 到 Memory 的提取](0024-session-to-memory-extraction.md) | 保留原始 Session，以规则、Tool 或可选后台任务生成有来源的长期记忆 |
| [0025 内置 Tool 边界](0025-builtin-tools-boundary.md) | 默认零内置 Tool；按需提供受限的 Skill/Memory 工具，摘要归内部任务 |
| [0026 Session 文件 I/O 适配器](0026-session-file-io-adapters.md) | 共用 JSONL 后端，按需提供可选文件适配器；平台差异只做薄层 |
| [0027 最小 Markdown Memory 布局](0027-minimal-markdown-memory-layout.md) | Soul、User、长期 Memory 和每日笔记四类文件的作用域、加载与更新边界 |
| [0028 Skill 投影双模式](0028-skill-projection-modes.md) | 注册期以 enum 声明 INLINE 全文或 ON_DEMAND 摘要；摘要层优先保障，读取 Tool 留在核心外 |
| [0029 官方可选 Skill 文件加载器](0029-official-skill-file-loader.md) | 可选包两层结构、三触发条件后实现；默认严格回滚、best_effort 显式选择；顺序/信任/查询前置齐备 |
| [0030 可选共享文件 I/O 边界](0030-shared-file-io-boundary.md) | 共享物理 I/O 的早期评估；后续平台预制方案与迁移节奏见 ADR 0031 |
| [0031 预制平台文件存储与读取](0031-prefabricated-platform-file-storage.md) | 提案：统一可选文件契约、ESP-IDF/OpenVela 预制实现、领域接入与分能力保证；用 JSONL Session 和 USER 读取验证 |

新增 ADR 时：沿用四位递增编号与 `NNNN-kebab-case.md` 命名，并将条目追加到
上表。
