# cAgentV2

cAgentV2 是面向嵌入式与 Host 的跨平台 C 语言 Agent 库，目标是在 ESP-IDF、
OpenVela、RT-Thread 等系统上复用同一套应用接口。项目采用平台无关的 Core、
可选 Model Provider 和平台 Port；容量在构建期确定，实例资源与依赖在初始化期
提供。

> **当前仍处于基础实现阶段**：Core workspace、生命周期、Runtime/Transport 契约、
> ESP-IDF/OpenVela 适配器、私有 JSON codec、OpenAI 非流式 Provider 与
> Session RAM Storage 契约已有代码和 Host 测试；同步 `agent_run()` 尚未实现
> ReAct 流程，文件系统 Session 持久化尚未实现。目前不能用它完成端到端 LLM 对话。

## 架构

```text
Application
  |  构建配置、workspace、Tool、Model 与平台依赖
  v
Public API (include/agent.h, include/agent/*.h)
  |
  +-- Core (src/)                 生命周期、运行契约、注册与资源边界
  |     +-- Model contract        调用 Provider 的 ops，不处理厂商协议
  |     +-- Runtime contract      单调时钟、可选平台服务
  |     +-- Transport contract    HTTP 请求/响应与流式接收接口
  |
  +-- Model Providers (providers/)  OpenAI 等协议实现，可选编译
  |     +-- JSON codec (codecs/json/)  私有有界读写器，可选编译
  |     +-- Transport ops
  |
  +-- Platform Ports (ports/)     Runtime 与 HTTP/TLS 的平台实现
```

上图表示模块职责，不代表所有执行路径均已实现。Core 不直接包含平台 SDK、
HTTP/TLS 实现或 OpenAI JSON 格式。应用在构建时选择所需组件，通过
`agent_config_t` 注入 Runtime，通过 `agent_set_model()` 绑定 Model；需要网络的
Model Provider 自行持有 Transport。

架构与模块边界的完整说明见[总体架构设计](architecture.md)。
Session 的现有接口与数据流见[开发日志](development/session.md)。

## 资源与配置

- 构建期通过 CMake 变量、可选 Kconfig 或统一 C 配置宏确定 Core 容量；Kconfig
  不是普通 CMake 构建的强制依赖。
- `agent_init()` 使用应用提供的 `agent_workspace_t`；`agent_create()` 可通过
  应用提供的 Runtime allocator 申请同样的 workspace。Core 在其中放置实例状态
  和每轮可复用的 scratch。Model Provider 与平台 Transport 的状态和缓冲不计入
  Core workspace，应分别预算。
- `agent_config_t` 提供实例级 Runtime、默认 limits 和系统提示词；limits 限制
  执行行为，不会扩大构建期确定的物理容量。
- `codecs/json/` 由调用方提供输入、token 数组和输出缓冲，不调用 heap allocator。
  这不意味着 Transport、TLS 或整个应用零 heap。

容量及所有权的设计细节见 [ADR 0011 配置边界](adr/0011-configuration-boundaries.md)、
[ADR 0014 内存域](adr/0014-memory-domains.md) 与
[ADR 0022 有界 JSON codec](adr/0022-bounded-json-codec.md)。

## 构建

平台无关的 Core 使用标准 CMake 构建：

```sh
cmake -S . -B build
cmake --build build
```

按需覆盖构建期容量或单独构建 JSON codec：

```sh
cmake -S . -B build -DCONFIG_AGENT_MAX_TOOLS=16 -DAGENT_BUILD_JSON_CODEC=ON
cmake --build build
```

普通 CMake 不会自动编译平台 Port 或 Provider。ESP-IDF 的 Core 和 Port 组件由
应用显式纳入构建；OpenVela Port 使用其 NuttX 构建入口。

## 验证

```sh
bash tests/headers/compile.sh
bash tests/core/compile.sh
bash tests/json/compile.sh
bash tests/transport/compile.sh
bash tests/ports/espidf/compile.sh
bash tests/ports/openvela/compile.sh
bash tests/session/compile.sh
```

这些脚本使用 Host 编译器和模拟平台头文件验证当前契约；JSON 测试还检查与应用
自带 jsmn 的链接共存。它们不能替代真实设备上的网络、TLS、栈与峰值内存测试。

## 许可证

本项目采用 MIT License。
