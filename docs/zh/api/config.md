# 配置与容量参考

公共头：`agent/config.h`，默认 limits 宏位于 `agent/types.h`。
配置应用与库必须一致，详见[构建指南](../getting-started/build.md)。

## 实例配置与存储

`agent_config_default()` 返回零初始化的实例配置和默认 limits。
应用必须提供 Runtime 的单调 now_ms，默认函数不会自动选择平台。

| 类型/字段 | 含义 |
|---|---|
| agent_config_t.system_prompt | 借用的固定系统指令 |
| agent_config_t.limits | 实例默认行为预算 |
| agent_config_t.runtime | 拷贝服务表，context 仍借用 |
| agent_workspace_t | 带保守对齐的 Core 私有字节存储 |
| agent_model_workspace_t | 独立 Model wrapper 存储，不包含 Provider 缓冲 |

Workspace 在 init 参数提供，不嵌入 config。当前没有公共 plan API 或运行期扩容 API。

## 物理容量宏

下表列出公共宏默认值；CMake/Kconfig 使用对应的 `CONFIG_AGENT_*` 名称。

| 宏 | 默认值 | 使用目的 |
|---|---:|---|
| AGENT_MAX_TOOLS | 12 | 注册 Tool 槽位，0 可裁剪 |
| AGENT_MAX_SKILLS | 8 | 注册 Skill 槽位，0 可裁剪 |
| AGENT_MAX_CONTEXTS | 8 | 动态 Context 与 Memory 选择共用槽位 |
| AGENT_SCRATCH_BYTES | 12288 | 每轮共享临时区域 |
| AGENT_MAX_PROJECTED_MESSAGES | 32 | 模型消息描述符上限 |
| AGENT_MAX_INPUT_BYTES | 1024 | 用户/参考等相应输入正文准入 |
| AGENT_MAX_CONTEXT_BYTES | 4096 | Context 文本预算 |
| AGENT_MAX_SCHEMA_BYTES | 2048 | 单个 Tool Schema |
| AGENT_MAX_ARGUMENTS_BYTES | 1024 | 单个 Tool 参数对象 |
| AGENT_MAX_TOOL_OUTPUT_BYTES | 1024 | 单次工具结果 |
| AGENT_MAX_MODEL_OUTPUT_BYTES | 2048 | 模型文本输出 |
| AGENT_MAX_MODEL_TOOL_CALLS | 4 | 单个模型响应的完整调用数 |
| AGENT_MAX_NAME_BYTES | 64 | 名称正文 |
| AGENT_MAX_DESCRIPTION_BYTES | 256 | 描述正文 |
| AGENT_MAX_IDENTIFIER_BYTES | 64 | Session/Trace/Call 等标识 |
| AGENT_MAX_JSON_DEPTH | 16 | Core Tool JSON 深度，有效范围 1..32 |
| AGENT_CORE_WORKSPACE_BYTES | 32768 | Core 总 Workspace，编译期校验布局 |
| AGENT_MODEL_WORKSPACE_BYTES | 128 | Model wrapper Workspace |

`AGENT_MAX_SESSIONS=4`、`AGENT_SESSION_EVENT_CAPACITY=96`、
`AGENT_SESSION_PAYLOAD_BYTES=8192` 仍出现在配置入口，
但当前 Core/Storage 实现没有用它们分配历史池。不要用它们估算 RAM/JSONL 容量；
这两个 Provider 的容量由各自 config 中的应用数组/缓冲决定。

“Generated workspace size”是构建配置发布的总大小，不是自动根据所有宏求出的公式。
当前 CMake 生成配置头并使用配置值；内部编译断言保证实际布局不超过它。
扩大槽位或 scratch 后仍须检查总 Workspace 配置。

## 默认行为宏

| 宏 | 默认值 |
|---|---:|
| AGENT_DEFAULT_MAX_STEPS | 8 |
| AGENT_DEFAULT_TIMEOUT_MS | 30000 |
| AGENT_DEFAULT_MODEL_TIMEOUT_MS | 15000 |
| AGENT_DEFAULT_TOOL_TIMEOUT_MS | 3000 |
| AGENT_DEFAULT_MAX_TOOL_CALLS | 4 |
| AGENT_DEFAULT_MAX_OUTPUT_TOKENS | 512 |
| AGENT_DEFAULT_MAX_HISTORY_TURNS | 0 |

这些宏只决定初始 limits，可通过空闲 set_limits 或请求完整覆盖。
它们不扩大物理容量，也不是不能放宽的产品硬安全策略。
