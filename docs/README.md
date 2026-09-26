# cAgentV2 文档索引

本目录保存 cAgentV2 的架构、接口和演进文档。cAgentV2 以当前 cAGENT V1 为行为
参考，目标是构建一个静态可组合、资源可预算、类型安全、运行可追踪的嵌入式 Agent
微内核，可由 ESP-IDF、openvela、RT-Thread 和 Host 系统的不同 port 使用同一套
公共 API 和行为契约。

## 仓库状态（2026-09）

本仓库仍处于架构与公共 API 草案阶段：`include/` 已补齐首批接口声明和简短普通注释，
`src/` 内部头已定义协作边界；Memory、通用 Plugin 与持久化 Storage 仍延后。
`.c` 实现、完整构建系统、运行测试基座和具体平台 port 尚未落地。新增的
`tests/headers/compile.sh` 仅检查头文件独立包含、组合包含及 C99/C++11 用法，不证明
运行行为。因此，这些声明不应被理解为已可链接运行或 ABI 稳定的实现。

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

## 文档原则

- 先定义边界、所有权和失败语义，再实现模块。
- 公共 API 草案在实现前必须完成使用示例和生命周期检查。
- V1 作为行为参考和迁移来源，不直接复制未经测试的实现。
- 文档中的接口在进入公共头文件前仍可调整；稳定后需要遵守 ABI 兼容策略。
