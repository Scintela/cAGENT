# ADR 0021: 复用平台 cJSON 的构建与模块边界

- 状态：未采纳为默认方案；见 [ADR 0022](0022-bounded-json-codec.md)
- 日期：2026-09-28
- 关联：[ADR 0006](0006-json-integration.md)、[ADR 0014](0014-memory-domains.md)

本文保留平台 cJSON 复用方案的评估记录；当前已实现的默认候选是私有 jsmn
codec。cJSON 后端尚未实现，不能通过构建选项选择。

## 背景

首批目标是 ESP-IDF、openvela 和 RT-Thread，三者均有可选的 cJSON 集成途径，
但不能假设任意目标工程都已经启用它：

| 平台 | 由应用选择的依赖入口 |
|------|----------------------|
| ESP-IDF 5.x | 内置 `json` 组件。 |
| ESP-IDF 6.x | `espressif/cjson` 托管组件；不再依赖内置 `json`。 |
| openvela | `CONFIG_NETUTILS_CJSON`，由 apps 构建纳入 cJSON 源码。 |
| RT-Thread | `PKG_USING_CJSON`，由软件包构建纳入 cJSON 源码。 |
| Host | 由使用方选择兼容的 cJSON 包或明确提供构建目标。 |

依据：[ESP-IDF 6.0 迁移说明](https://github.com/espressif/esp-idf/blob/master/docs/en/migration-guides/release-6.x/6.0/protocols.rst)、
[openvela cJSON 构建](https://github.com/open-vela/nuttx-apps/blob/dev/netutils/cjson/CMakeLists.txt)、
[RT-Thread cJSON 软件包](https://github.com/RT-Thread-packages/cJSON/blob/master/SConscript)。

当前 `cagent_core` 的源码列表包含 `src/tool/tool_schema.c`，但该文件仍是占位实现；
`providers/model/openai` 使用私有 JSON codec，未接入 cJSON。公共 `agent_tool_t` 与
`agent_tool_view_t` 目前提供借用的 `input_schema_json`，**没有**已经实现的类型化
schema descriptor。这与 ADR 0006 中“类型化 descriptor 是规范来源”的目标不同，
不能把目标设计写成当前行为。

## 决定（采纳后）

1. cJSON 是可选 JSON 功能模块的**构建期依赖**，不是 `cagent_core`、Runtime、
   Transport 或公共头文件的必需依赖。只构建 Core 时不要求安装 cJSON。
2. 应用选择并启用平台提供的 cJSON；需要 JSON 的 Model Provider 或 Storage 模块
   依赖同一份实现。cAGENT 不默认复制、下载或编译第二份 `cJSON.c`，不在运行期注入
   JSON ops，也不向公共 API 泄露 `cJSON *`。
3. JSON 线格式的生成与解析归协议拥有者：OpenAI/Anthropic Provider 构造各自的
   HTTP body、Tool 定义和模型响应；JSONL Storage 处理持久化格式。Core 的
   `src/tool/tool_schema.c` 不直接包含 `cJSON.h` 或实现某一模型的 wire format。
   如需公共 Tool 约束校验，保持与具体 JSON 库无关。
4. MVP 延续现有 `input_schema_json` 契约：注册信息和 Model 请求借用完整、有界的
   JSON Schema 对象；Provider 在发送请求前检查其 JSON 语法与预期顶层类型，
   失败即返回错误，不发送请求。Tool arguments 的 JSON 解析归 Provider，
   Tool 参数的业务/设备约束仍由 Tool 层校验。类型化 descriptor 是否取代
   `input_schema_json` 需要独立裁决，不在本 ADR 中暗改公共 API。
5. 平台集成层只负责启用依赖、提供正确的头文件搜索路径与链接目标，不再包一层
   JSON VTable。Core 不根据 `ESP_PLATFORM` 等宏选择 JSON 实现。

依赖方向：

```text
Application build -> one platform cJSON component (when JSON features are enabled)
Provider / JSONL Storage -> that cJSON component
Provider -> cagent_core contracts
cagent_core / public headers -> no cJSON dependency
```

## 构建与版本规则

- 各平台的 Kconfig、组件清单或 SCons/CMake 由应用集成层维护；Core 的通用构建
  不强制运行 Kconfig，也不自动打开 JSON 功能。
- 只在启用相应 Provider/Storage 时检查 cJSON 是否可用；缺失依赖应在配置或
  链接时明确失败，不能静默编入另一份实现。ESP-IDF 5.x 与 6.x 的依赖声明分别
  适配，不用一个固定组件名假装两者相同。
- Provider 应只使用目标组件实际提供的 cJSON API，并用该组件的头文件编译。
  平台包版本可能不同；最低支持版本/API 在实现时以三个目标构建验证确定。
- 集成测试检查最终固件的链接映射或符号表，确认没有两份互相竞争的
  `cJSON_*` 实现。静态库链接即使没有报重复定义，也不能视为版本一致的证明。
- Host 测试显式提供 cJSON 依赖；默认 Core-only 构建不得因为没有 cJSON 失败。

## 内存与所有权

复用平台 cJSON 不等于零 heap。cJSON 解析和 DOM 构造可能动态分配；
`cJSON_PrintPreallocated()` 只使**输出缓冲**由调用方提供，不使 DOM 构造变成
零分配。Provider/Storage 按 ADR 0006 对输入大小、嵌套、对象寿命和失败路径
设上限，并测量峰值 heap；Core workspace 不替外部库承担这些分配。

解析得到的树由调用方用同一 cJSON 实现释放；借用的 schema 字符串在 Provider
完成请求前必须保持有效。`cJSON_InitHooks()` 属于全局配置，不允许按 Agent
实例或每轮调用切换。需要严格无 heap 的目标，应另做有界 codec 或 Provider，
不能假称复用平台 cJSON 已满足该要求。

## 与 ADR 0006 的关系

本提案保留 ADR 0006“公共 API 不暴露 cJSON、外围模块承担 JSON 分配”的原则，
并在采纳后收紧两点：

- “内部 schema 模块可用 cJSON”限定为 Core **之外**的协议投影代码；
  `src/tool/tool_schema.c` 不能因此把 cJSON 带进 Core。
- “类型化 descriptor 为规范来源”仍是候选演进方向；在公共接口实际改动前，
  MVP 的事实来源是 `input_schema_json`，不能用尚不存在的 descriptor 推断校验能力。

## 备选方案与代价

| 方案 | 取舍 |
|------|------|
| cAGENT 内置固定版 cJSON | 各平台行为一致，但容易与应用已有实现冲突或重复，并扩大 Core/发行包的依赖。 |
| 所有 JSON 由应用回调实现 | 可彻底复用应用自选库，但公共接口与所有权规则明显变复杂，首版没有必要。 |
| Core 直接依赖平台 cJSON | 实现方便，却让无模型、mock 和非 JSON 目标也承担依赖；与 Core 边界冲突。 |
| 手写完整 JSON codec | 可控制内存，但需要持续承担转义、Unicode、嵌套与畸形输入的正确性及测试成本。 |

## 验证要求

- Core-only 在无 cJSON 的 Host 环境可构建；启用 Provider 而缺依赖时明确失败。
- ESP-IDF 5.x/6.x、openvela、RT-Thread 各用其目标组件完成一次链接验证；
  比对 cJSON 头文件、实现和最终映射，避免两份实现。
- schema 非法、顶层类型错误、超限、转义或嵌套 arguments、模型错误响应和
  取消路径均返回明确错误且不泄漏 cJSON 对象。
- 在目标设备测 Provider 请求/响应的峰值 heap、最大栈和固件体积；
  不以 cJSON 源码行数推断实际链接体积。
