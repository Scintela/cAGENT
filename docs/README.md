# cAgentV2 文档索引

本目录保存 cAgentV2 的架构、接口和演进文档。cAgentV2 以当前 cAGENT V1 为行为
参考，目标是构建一个静态可组合、资源可预算、类型安全、运行可追踪的嵌入式 Agent
微内核，可由 ESP-IDF、openvela、RT-Thread 和 Host 系统的不同 port 使用同一套
公共 API 和行为契约。

## 仓库状态（2026-09）

本仓库仍在 MVP 实现阶段：Core 生命周期、部分 Model/Transport 封装、ESP-IDF 与
OpenVela Port 已有实现和 mock 测试；`agent_run()` 的完整 Model/Tool/Session 执行链
仍未落地。Memory、通用 Plugin 与持久化 Storage 继续延后。头文件可编译不代表所有
声明已可链接运行或 ABI 稳定，设备上的网络与 TLS 行为仍需验证。

运行头文件检查：`bash tests/headers/compile.sh`；可通过 `CC`、`CXX` 选择编译器。
接口收敛以 [public-api.md](api/public-api.md) 及其头文件快照说明为准；总体架构和
ADR 0006 中尚未同步的设计差异仍需评审，尤其是 Tool schema 的权威输入形式。

## 当前文档

| 文档 | 内容 |
|------|------|
| [architecture.md](architecture.md) | V2 总体架构、核心概念、模块边界、生命周期、内存模型、API 草案和迁移计划 |
| [api/public-api.md](api/public-api.md) | 目标公共 API 清单、当前落地状态与待决接口 |
| [arch/README.md](arch/README.md) | 模块责任地图与后续模块文档计划 |
| [adr/0006-json-integration.md](adr/0006-json-integration.md) | cJSON 集成边界、内存策略与 Tool/OpenAI JSON 处理决定 |
| [adr/0007-http-transport-adapters.md](adr/0007-http-transport-adapters.md) | HTTP Transport 的平台 Adapter、裁剪、同步语义和所有权提案 |
| [adr/0008-error-contract.md](adr/0008-error-contract.md) | 跨平台错误类别、来源、远端失败、Tool 副作用事实与传播规则提案 |
| [adr/0009-types-boundary.md](adr/0009-types-boundary.md) | 公共 `types.h` 的跨模块类型边界、Model 类型迁移与 include 规则提案 |
| [adr/0010-context-projection.md](adr/0010-context-projection.md) | Context 的编排、模型投影、资源预算、Memory 边界与 Provider 契约提案 |
| [adr/0011-configuration-boundaries.md](adr/0011-configuration-boundaries.md) | 编译期容量 Profile、运行期 limits、workspace 与可替换 Model Provider 的边界提案 |
| [adr/0012-agent-header-boundary.md](adr/0012-agent-header-boundary.md) | `agent.h` 的应用入口、聚合范围、生命周期与同步运行边界提案 |
| [adr/0013-text-representation.md](adr/0013-text-representation.md) | 公共 API 文本表示候选方案：NUL 字符串、长度视图、混合边界、双轨 API 与 `_Generic` |
| [adr/0014-memory-domains.md](adr/0014-memory-domains.md) | Core workspace、Session、turn scratch、Provider/Transport 与外部库的内存域和分配边界提案 |
| [adr/0015-event-model.md](adr/0015-event-model.md) | Live Event 观测模型：边界事件集合、单回调契约、push/pull 分工、双通道与 OTel 映射提案 |
| [adr/0016-session-history-storage.md](adr/0016-session-history-storage.md) | Session 完整 turn 历史的投影窗口、Storage Provider 所有权、JSONL/Flash/NVS 持久化与 PSRAM 缓存边界提案 |
| [adr/0017-runtime-portability.md](adr/0017-runtime-portability.md) | Runtime 最小平台服务、时钟/同步/allocator 契约和跨系统 Port 组织方案提案 |
| [adr/0018-transport-portability.md](adr/0018-transport-portability.md) | HTTP/TLS Transport 通用契约、平台 Adapter、构建裁剪与 Provider 分层方案提案 |
| [adr/0019-kconfig-integration.md](adr/0019-kconfig-integration.md) | Kconfig 集成边界：Core Profile 菜单、Port 包裁剪项、命名规范与等价通道提案 |
| [adr/0020-synchronous-run-mvp.md](adr/0020-synchronous-run-mvp.md) | 同步运行 MVP 的公开边界、取消和确认失败关闭规则 |

## 文档原则

- 先定义边界、所有权和失败语义，再实现模块。
- 公共 API 草案在实现前必须完成使用示例和生命周期检查。
- V1 作为行为参考和迁移来源，不直接复制未经测试的实现。
- 文档中的接口在进入公共头文件前仍可调整；稳定后需要遵守 ABI 兼容策略。
