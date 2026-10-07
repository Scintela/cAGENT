# ADR 0019: Kconfig 集成边界、Core Profile 与 Port 裁剪

- 状态：已采纳，首版构建集成已实现
- 日期：2026-09-27

## 背景

`agent/config.h` 已定义三层编译期容量来源：直接定义的 `AGENT_*`、构建系统生成的
`CONFIG_AGENT_*` 与内建默认值。ADR 0011 将 Core 容量 Profile、运行期 limits 和
caller-owned workspace 分开；ADR 0017/0018 将平台 SDK、Runtime 与 HTTP/TLS Adapter 放入
独立 `ports/` 包。

当前仍有四个缺口：

- 仓库没有 Kconfig 或组件 CMake，Tiny、Default、Device ReAct Profile 没有可执行载体；
- `config.h` 只会读取已经可见的 `CONFIG_AGENT_*`，却没有平台无关的生成配置头注入机制；
- `ENABLE_TOOL/SESSION/CONTEXT/SKILL` 与默认 limits 宏尚未落地；
- Port、Provider、codec 与 Core Profile 的 Kconfig 所有权尚未固定，容易重新让 Core 依赖平台。

本 ADR 规定 Kconfig 的责任边界与后续实现顺序。Kconfig 是 ESP-IDF、NuttX/OpenVela 等构建系统的
一个集成通道，不是 cAgent 的唯一配置机制。

## 决定

### 1. Kconfig 只表达构建事实，不取代实例配置

| 类别 | 是否进入 Kconfig | 说明 |
|---|---|---|
| Core 模块是否编译 | 是 | 例如 Tool、Context、Skill、Session 的构建裁剪；实现前不得声明已生效。 |
| Core Profile 与物理容量 | 是 | Profile 一次性生成 Core persistent、scratch、admission 与 workspace 参数。 |
| 每个实例的默认 limits | 可选 | 是固件默认行为，应用仍可用 `agent_set_limits()` 或 request 覆盖。 |
| Port Runtime/Transport 是否编译 | 是 | 只由对应 `ports/<platform>` 包声明。 |
| Provider、JSON codec 是否编译 | 是 | 由 Provider/codec 自己的可选组件声明，不属于 Core 菜单。 |
| workspace、URL、证书、缓冲区、API key | 否 | 属于 application/Provider/Transport 实例配置和内存预算。 |
| per-turn timeout、Tool 调用数、输入内容 | 否 | 属于运行期 `agent_limits_t` 或 `agent_request_t`。 |

Kconfig 不得被用作运行时 backend 选择器、凭证存储或每个 Agent 的容量表。Kconfig 选择的是
同一固件内所有 Core 实例共享的编译布局；`agent_workspace_t` 的大小因此在整个固件中一致。

### 2. 生成配置头是唯一的 Kconfig 到 Core 传递边界

Core 不能直接包含 ESP-IDF 的 `sdkconfig.h`，也不能检测任何平台宏。否则 public header 会反向依赖
特定 SDK，破坏 `src/` 与 `ports/` 的边界。

Core 采用可选、平台无关的生成配置头注入机制：

```c
#if defined(AGENT_BUILD_CONFIG_HEADER)
#include AGENT_BUILD_CONFIG_HEADER
#endif
```

构建系统在编译 Core 之前定义 `AGENT_BUILD_CONFIG_HEADER`，并生成该头；头中只定义
`CONFIG_AGENT_*`。ESP-IDF 的 component CMake 可以从 `sdkconfig` 生成此头，NuttX/OpenVela、
纯 CMake、PlatformIO 或手写 Profile 也可生成等价头。

`agent/config.h` 继续遵循：直接 `AGENT_*` > 生成的 `CONFIG_AGENT_*` > 内建默认值。直接宏是
无 Kconfig 构建、测试和临时源码集成的逃生口，不应在一个固件的不同 translation unit 中给出不同
值。构建系统必须保证所有编译 Core 的 translation unit 看见同一份 Profile；若同时使用直接宏与
生成头，应在 CI 中报告覆盖告警，而不是假定 menuconfig 值一定生效。

### 3. Core、Port、Provider 各自拥有 Kconfig

Kconfig 必须位于其所属构建组件根，而不是抽象地假设仓库根总是组件根：

```text
cagent Core component/
  CMakeLists.txt
  Kconfig                         # Core Profile、Core module、默认 limits

ports/espidf/
  CMakeLists.txt
  Kconfig                         # ESP-IDF Runtime/Transport source selection

providers/model/openai/
  CMakeLists.txt
  Kconfig                         # OpenAI Provider 与所需 codec

codecs/cjson/
  CMakeLists.txt
  Kconfig                         # cJSON integration, if delivered as a component
```

- Core Kconfig 不出现平台名、SDK 依赖、HTTP client、TLS、Wi-Fi 或芯片 target；
- Port Kconfig 只选择自己的源码与平台能力，不修改 Core 容量 Profile；
- Provider/codec Kconfig 不依赖某一个 Port。OpenAI Provider 只在运行时绑定
  `agent_transport_t`，可使用 ESP-IDF、Host 或应用自定义 Transport；
- Mock 是测试 Provider，默认不进入生产 Core；若以组件交付，其 Kconfig 归测试/Mock 包；
- Storage Provider 的 RAM cache 与持久化策略拥有独立 Profile，不进入 Core workspace Profile。

这允许没有网络的 Runtime-only 产品不链接 `esp_http_client`，也允许仅以 application 自定义
Transport 驱动联网 Provider。
Port 构建开关只决定哪些 Backend 源码和依赖进入固件。单平台产品通常只开启一个官方 HTTP
Backend，但不要求所有 Backend 选项互斥；同一固件可为不同 Provider/Tool 初始化多个
`agent_transport_t` 实例。具体实例在初始化期注入，Core 不维护全局 HTTP Backend 选择器。

### 4. Core Profile 菜单以 choice 为主，Custom 才暴露细项

Core Kconfig 的目标形态如下。它是菜单语义，不是当前已存在的文件：

```text
menu "cAgent Core"
  choice AGENT_PROFILE
    prompt "Core profile"
    default AGENT_PROFILE_DEFAULT

    config AGENT_PROFILE_TINY
      bool "Tiny"
    config AGENT_PROFILE_DEFAULT
      bool "Default"
    config AGENT_PROFILE_DEVICE_REACT
      bool "Device ReAct"
    config AGENT_PROFILE_CUSTOM
      bool "Custom"
  endchoice

  # 每个 CONFIG_AGENT_* 容量项没有普通 prompt，按 profile 条件 default 取值。
  # 仅 CUSTOM 为对应容量项打开 prompt。

  # 可选的固件默认行为：DEFAULT_* 七项。
endmenu
```

普通应用只选择 Profile。每个容量符号使用按 `AGENT_PROFILE_*` 条件生效的 `default` 值；当选择
`CUSTOM` 时，才为对应符号打开 prompt 和 range。Kconfig 的 `choice` 不直接“生成一组隐藏项”，
而是由每个隐藏 `int`/`bool` symbol 的条件默认值形成完整 Profile。这样 `sdkconfig` 仍能审计最终值。

Core Profile 分为以下类别，具体符号清单以 `config.h` 与 ADR 0011 为准，不在本 ADR 固定“十九项”
之类容易漂移的数量：

| 类别 | 典型项 | 所有权 |
|---|---|---|
| Core persistent | Tool、Context、Skill registry 槽位 | Core Profile |
| Turn scratch | `SCRATCH_BYTES`、`MAX_PROJECTED_MESSAGES` | Core Profile |
| Admission cap | input/context/schema/arguments/output/name/json-depth 上限 | Core Profile |
| Generated workspace | `CORE_WORKSPACE_BYTES`、通用 Model wrapper workspace | Core/Profile 构建生成 |
| Storage cache | Session RAM cache、event/payload 容量 | Session Storage Provider Profile，不属于 Core |

Profile 不能只修改 `CORE_WORKSPACE_BYTES`。每项容量与 workspace 总大小必须在同一 Profile 中生成，
并由 Core 的编译期布局断言验证；不能把布局不足推迟成随机的运行期失败。

### 5. 默认 limits 有七项，且仍可在运行期覆盖

当前 `agent_limits_t` 包含七项，应对应下列可选构建默认宏：

```text
CONFIG_AGENT_DEFAULT_MAX_STEPS
CONFIG_AGENT_DEFAULT_TIMEOUT_MS
CONFIG_AGENT_DEFAULT_MODEL_TIMEOUT_MS
CONFIG_AGENT_DEFAULT_TOOL_TIMEOUT_MS
CONFIG_AGENT_DEFAULT_MAX_TOOL_CALLS
CONFIG_AGENT_DEFAULT_MAX_OUTPUT_TOKENS
CONFIG_AGENT_DEFAULT_MAX_HISTORY_TURNS
```

后续 `AGENT_LIMITS_DEFAULT` 和 `agent_config_default()` 应引用与这些值对应的 `AGENT_DEFAULT_*`
三级宏。Kconfig 仅确定新建 Agent 的固件默认行为；`agent_set_limits()` 和 request-level limits
仍可在不改变物理容量的前提下覆盖它们。

### 6. 命名与 ESP-IDF Port 裁剪

| 前缀 | 归属 | 示例 |
|---|---|---|
| `CONFIG_AGENT_` | Core Profile 容量与默认 limits | `CONFIG_AGENT_MAX_TOOLS`、`CONFIG_AGENT_DEFAULT_MAX_STEPS` |
| `CONFIG_AGENT_ENABLE_` | Core 模块编译裁剪 | `CONFIG_AGENT_ENABLE_TOOL` |
| `CONFIG_AGENT_PORT_<PLATFORM>_` | 对应 Port 的构建选择 | `CONFIG_AGENT_PORT_ESPIDF_RUNTIME` |
| `CONFIG_AGENT_PROVIDER_` | 可选 Model/Provider 组件 | `CONFIG_AGENT_PROVIDER_OPENAI` |
| `CONFIG_AGENT_CODEC_` | 可选 codec 组件 | `CONFIG_AGENT_CODEC_CJSON` |

ESP-IDF 首版建议使用一个 `ports/espidf` component，保留 `runtime/` 与 `transport/` 的源码边界，
由 Kconfig 选择其源文件：

```kconfig
config AGENT_PORT_ESPIDF_RUNTIME
    bool "Enable ESP-IDF Runtime builder"
    default y

config AGENT_PORT_ESPIDF_TRANSPORT
    bool "Enable ESP-IDF HTTP Transport"
    default n
```

其 CMake 应仅在相应选项开启时加入 source 与私有 SDK 依赖。Port 的公开头包含 cAgent 类型，
因此 Port 对 Core 是公开 `REQUIRES`；`esp_timer`、`esp_http_client` 是仅实现使用的
`PRIV_REQUIRES`。Core component 名称建议固定为 `cagent`，但只有在 Core component CMake 可构建后
才能把此名称写入 Port CMake。

## 首版实现状态

- `agent/config.h` 与 `agent/types.h` 可通过 `AGENT_BUILD_CONFIG_HEADER` 读取构建生成的统一配置头；
- `AGENT_DEFAULT_*` 默认值支持七项限额，并由 `agent_config_default()` 使用；
- 根 `CMakeLists.txt` 构建平台无关的 `cagent_core`，Mock 与 OpenAI Provider 可选择性构建；
- 根 `Kconfig` 定义 Tiny、Default、Device ReAct、Custom Profile 及现有容量/default-limit 符号；
- `ports/espidf` 通过独立 Kconfig/CMake 选择 Runtime 和 HTTP Transport Adapter；
- 当前 Core workspace 编译期断言仍只验证 `sizeof(agent_t) + AGENT_SCRATCH_BYTES`，后续应结合真实布局和 profile 继续加强；
- Core 子模块逐项裁剪尚未实现，因此首版 Kconfig 不暴露会造成“菜单可关、源码仍编译”的 ENABLE 开关；
- OpenVela 已有可独立选择的单调时钟 Runtime 和 `webclient` Transport Kconfig/CMake 片段，
  但尚未在目标系统完成组件集成与设备验证；
- RT-Thread 已添加独立 Kconfig、SConscript 与可选 CMake targets，分别裁剪 Runtime、
  WebClient POST 和 DFS/POSIX 文件绑定。SCons 使用 rtconfig.h 的原生 AGENT_* 宏，
  CMake 使用 Core 生成的统一配置头。Host 构建/链接检查通过，不等于完整 BSP 验证。

Profile 数值目前是可用的初始值，仍需在目标 MCU 上测量 workspace 峰值，并据此校准。

## 不采用的方案

### 将所有容量项平铺为一级菜单

普通用户会被大量容量项淹没，Profile 形同虚设。采用 Profile choice 与 CUSTOM 才展开细项。

### Core 直接包含 `sdkconfig.h` 或识别平台宏

会把 ESP-IDF 反向带入 Core public header，破坏无 Kconfig、NuttX、Host 和其他构建系统的可移植性。
采用生成配置头注入。

### 平台选择项进入 Core Kconfig

会破坏 ADR 0017/0018 的 `ports/` 边界，并让 Core 重新承担平台枚举和 SDK 依赖；拒绝。

### Provider 依赖某一个 Transport Port

会阻止 application 自定义 Transport，也使可复用 Provider 变成平台专用实现；拒绝。

### 将 Provider、codec、凭证或实例 buffer 放入 Core Profile

Provider/codec 是可选组件，凭证与 buffer 是实例配置。混入 Core 会破坏内存账本与所有权边界；拒绝。

### 要求全部构建系统使用 Kconfig

直接 `AGENT_*` 宏和生成配置头是等价通道。Kconfig 只是其中一个 profile frontend；拒绝唯一化。

## 影响

正面影响：

- 普通产品以一个 Profile choice 获得可审计的 Core 容量组合；
- Core 不感知 ESP-IDF/NuttX，Port 也不能偷偷修改 Core 内存布局；
- Runtime-only 与 Transport-enabled ESP-IDF 固件可独立裁剪 SDK 依赖；
- Kconfig、CMake、纯 CMake 与手写配置头可通过同一 `CONFIG_AGENT_*` ABI 汇合；
- Provider、Transport、TLS 和 Storage 的内存继续与 Core workspace 分账。

代价：

- 需要维护生成配置头、Profile 数值表和编译矩阵；
- Port、Provider 与 Core component 的版本协同成为发布责任；
- Kconfig 只能确保单个构建配置一致，无法替代对 workspace、SDK heap、TLS 峰值的实测。

## 验证要求

- 每个 Profile 生成的 `CONFIG_AGENT_*` 均通过 Core 布局编译期断言；
- 同一 Profile 通过生成配置头与直接 `-DAGENT_*` 构建时，`sizeof(agent_workspace_t)` 与相关布局一致；
- Runtime-only ESP-IDF 构建不链接 `esp_http_client`、TLS 或 DNS；Transport-enabled 构建才引入
  对应私有依赖；
- Port、Provider 与 codec 的 Kconfig 不得写入 Core 容量符号；
- `CONFIG_AGENT_*`、`config.h` 消费的符号和 Profile 文档之间由脚本检查，避免孤儿或漂移项；
- ESP-IDF menuconfig 能发现 component 根 Kconfig；其他构建系统可完全绕开 Kconfig 并保持等价配置；
- 直接宏覆盖生成值时，CI 输出明确告警并检查所有 Core translation unit 使用同一组定义。
