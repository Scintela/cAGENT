# cAgentV2

cAgentV2 是面向 ESP-IDF、OpenVela、RT-Thread、STM32 和 Host 的轻量、可组合、资源可预算的 C
语言 Agent Core。当前已具备 Core workspace、Runtime callback、Model wrapper 和 HTTP Transport
Ops 的基础实现；ReAct、OpenAI Provider、Session Storage 和具体平台 Port 仍在设计或实现中。

## 文档

- [总体架构](docs/architecture.md)
- [公共 API 草案](docs/api/public-api.md)
- [ADR 索引](docs/README.md)

## 验证

```sh
bash tests/headers/compile.sh
bash tests/core/compile.sh
bash tests/transport/compile.sh
bash tests/ports/espidf/compile.sh
```

头文件检查覆盖公共头的独立和组合包含。Core 与 Transport 检查覆盖当前实现的生命周期和
同步分发契约；ESP-IDF Port 检查以 mock SDK 验证 Runtime 和 `esp_http_client` Adapter 契约，
不替代真实硬件、TLS 或网络集成测试。

## Port 包

Core 的通用代码位于 `src/`。平台 SDK、RTOS、HTTP/TLS 适配位于可选的
[ports/](ports/README.md) 包中，由产品构建系统显式选择；未选择的 Port 不会进入 Core 库。

## CMake 与 Kconfig

普通 CMake 构建只生成平台无关的 Core：

```sh
cmake -S . -B build -DCONFIG_AGENT_MAX_TOOLS=16
cmake --build build
```

容量与默认 limits 通过同一生成配置头传入所有依赖目标。ESP-IDF 工程将本仓库放在
`components/cagent`，并将 `ports/espidf` 加入组件搜索路径；Core 与 ESP-IDF Port 分别提供
Kconfig/CMake。Runtime 默认启用，HTTP Transport 默认关闭，需要时在 menuconfig 中启用。
当前 Mock/OpenAI Model provider 仍是骨架，CMake 选项仅用于独立编译对应源码，不代表协议实现已完成。

## 许可证

本项目采用 [MIT License](LICENSE)。
