# cAgentV2

cAgentV2 是面向嵌入式与 Host 的跨平台 C 语言 Agent 库，目标是在 ESP-IDF、
OpenVela、RT-Thread 等系统上复用同一套应用接口。项目采用平台无关的 Core、
可选 Model Provider 和平台 Port；容量在构建期确定，实例资源与依赖在初始化期提供。

**当前仍处于基础实现阶段**：Core workspace、生命周期、Runtime/Transport 契约、
ESP-IDF/OpenVela 适配器、私有 JSON codec 和 OpenAI 非流式 Provider 已有代码与
Host 测试；Session 已有格式无关的 Storage 契约和可选 RAM 后端，但文件持久化与同步
`agent_run()` 的 ReAct 流程尚未实现，Mock Provider 仍是占位。目前不能通过 Core 完成
端到端 LLM 对话。
Session 的 API、内存与数据流见[开发日志](docs/zh/development/session.md)。

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
Model Provider 自行持有 Transport。当前的 JSON codec 使用内嵌 jsmn 作为 tokenizer，
不要求系统提供 cJSON，也不向公共头文件暴露 JSON 类型。

## 目录

| 路径 | 职责 |
|------|------|
| [`include/`](include/) | 公共 C API 与构建容量配置。 |
| [`src/`](src/) | 平台无关的 Core、Model wrapper、Runtime/Transport 转发及各领域模块。部分模块尚未实现。 |
| [`providers/`](providers/) | 可选 Model 与 Session Storage Provider；OpenAI 支持非流式 Chat Completions，Session RAM 后端易失，Mock/Anthropic 尚未实现。 |
| [`codecs/json/`](codecs/json/) | 可选的有界 JSON reader/writer；上游 jsmn 位于 `vendor/jsmn/`。 |
| [`ports/`](ports/) | 可选的平台 Runtime 和 HTTP/TLS Adapter；ESP-IDF/OpenVela 已有实现，Host/RT-Thread/STM32 仍需完善。 |
| [`tests/`](tests/) | 公共头、Core、JSON、Transport 与 Port 的 Host 契约测试。 |
| [`docs/`](docs/) | 中文文档（`zh/`）与 Docusaurus 站点（`website/`），含全部 ADR；设计文档中的目标能力不等于已实现能力。 |

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

容量及所有权的设计细节见 [配置边界](docs/zh/adr/0011-configuration-boundaries.md)、
[内存域](docs/zh/adr/0014-memory-domains.md) 与
[JSON codec](docs/zh/adr/0022-bounded-json-codec.md)。

## 在不同系统和平台移植

应用只需使用 `include/` 的公共接口，不应包含 `src/` 的内部头文件。移植时先把
Core 加入产品构建，再提供 `agent_runtime_t.now_ms` 所需的单调毫秒时钟；日志、
allocator 和跨任务取消同步按需注入。使用联网 Model 时，再选择或实现符合
`agent_transport_ops_t` 的 HTTP Adapter，并在 Provider 中绑定；不需要网络的应用
可以不编译 Transport。

| 目标 | 当前接入方式 |
|------|--------------|
| Host / 普通 CMake | 默认只构建 Core；由应用实现 Runtime，按需启用 JSON codec 与 OpenAI Provider。尚无 Host HTTP Port。 |
| ESP-IDF | 将仓库作为 `components/cagent`，把 `ports/espidf` 加入组件搜索路径；由 Kconfig 分别选择 Runtime、基于 `esp_http_client` 的 Transport 与可选 OpenAI Provider。 |
| OpenVela / NuttX | 在应用 Kconfig 中引入 `ports/openvela/Kconfig`，在 NuttX 构建中加入 `ports/openvela`；按需选择 Runtime 和 `netutils/webclient` Transport。HTTPS 还需要应用提供验证证书链与主机名的 TLS 实现。 |
| RT-Thread / STM32 | Port 尚未实现；应用可自行填充 Runtime/Transport ops，并把平台实现放在 Core 之外。 |

Core 不强制依赖 Kconfig，也不会根据平台宏自动选择 Backend。平台适配所需的 SDK
头文件和链接依赖由相应 Port 承担；未选择的 Port 不进入 Core。具体构建入口见
[Port 集成说明](ports/README.md)、[ESP-IDF Port](ports/espidf/README.md) 和
[OpenVela Port](ports/openvela/README.md)。目前平台测试使用模拟 SDK；真实设备上的
HTTP/TLS、栈和峰值内存仍需由产品集成验证。

## 快速开始

在 Host 上先构建平台无关的 Core；按需覆盖容量并编译私有 JSON codec：

```sh
cmake -S . -B build -DCONFIG_AGENT_MAX_TOOLS=16 -DAGENT_BUILD_JSON_CODEC=ON
cmake --build build
```

应用通过 CMake 链接 `cagent::core` 目标，以继承库生成的配置头和相同的容量宏；
不要仅手工链接静态库却用另一组宏编译公共头文件。

应用使用调用方持有的 workspace 初始化 Core。下面的 `app_monotonic_ms` 由目标
系统实现，必须返回单调非递减的毫秒时间；ESP-IDF/OpenVela 也可使用对应的
Runtime builder 填充 `config.runtime`。

```c
#include <agent.h>

static agent_workspace_t workspace;

/* 由应用的平台层实现。 */
extern uint64_t app_monotonic_ms(void* context);

int main(void)
{
    agent_config_t config = agent_config_default();
    agent_t* agent = NULL;

    config.runtime.now_ms = app_monotonic_ms;
    if (agent_init(&agent, &workspace, &config) != AGENT_OK) {
        return 1;
    }
    if (agent_start(agent) != AGENT_OK) {
        agent_destroy(agent);
        return 2;
    }

    agent_destroy(agent);
    return 0;
}
```

接入平台时钟后，这是当前可运行的最小生命周期示例，并非对话示例；`agent_run()` 仍返回
`AGENT_ERROR_NOT_SUPPORTED`。完整的 Model/Tool 链路还需 ReAct 实现。
Provider 的配置和缓冲区契约见 [OpenAI Provider](providers/model/openai/README.md)。
现有可执行契约见 [Core 生命周期测试](tests/core/lifecycle.c)。

在 Host 上运行测试：

```sh
bash tests/headers/compile.sh
bash tests/core/compile.sh
bash tests/json/compile.sh
bash tests/transport/compile.sh
bash tests/ports/espidf/compile.sh
bash tests/ports/openvela/compile.sh
bash tests/providers/openai/compile.sh
bash tests/session/compile.sh
bash tests/session/jsonl_compile.sh
```

这些测试验证当前接口及模拟 Port 的行为，不能代替真实设备上的网络和 TLS 联调。

## 文档

- 在线文档站点：`https://<username>.github.io/cAgentV2/`（推送 GitHub 并启用
  Pages 后生效；本地预览与目录说明见 [docs/README.md](docs/README.md)）
- [总体架构设计](docs/zh/architecture.md)
- [公共 API 草案](docs/zh/api/public-api.md)
- [模块责任地图](docs/zh/arch/module-map.md)
- [ADR 索引](docs/zh/adr/index.md)

## 许可证

本项目采用 [MIT License](LICENSE)。
