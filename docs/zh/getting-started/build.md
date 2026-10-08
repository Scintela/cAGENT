# 构建与配置

cAgentV2 主要随应用源码一起编译。基础机制是 C 配置宏，普通 CMake 生成共享配置头，
Kconfig 是平台集成选项，不是跨平台 Core 的强制依赖。

## 普通 CMake

仅构建 Core：

```sh
cmake -S . -B build
cmake --build build
```

应用通过 `add_subdirectory()` 加入仓库，再链接相应目标。启用选项要在加入仓库前设置，
或通过 CMake 命令行传入；不要混用几份容量不同的库。

| 构建选项 | 导出的目标 | 依赖 |
|---|---|---|
| Core，默认构建 | `cagent::core` | 默认 Tool 容量非零时包含 JSON reader |
| `AGENT_BUILD_JSON_CODEC` | `cagent::json_jsmn` | 私有 JSON codec，不是应用 JSON API |
| `AGENT_BUILD_OPENAI_PROVIDER` | `cagent::provider_openai` | 完整 JSON codec |
| `AGENT_BUILD_SESSION_RAM` | `cagent::session_ram` | Core |
| `AGENT_BUILD_FILE_STORE` | `cagent::file_store` | 公共字节文件契约 |
| `AGENT_BUILD_SESSION_JSONL` | `cagent::session_jsonl` | 完整 JSON codec |
| JSONL + File Store | `cagent::session_jsonl_files` | Session 文件命名桥接 |
| `AGENT_BUILD_MARKDOWN_MEMORY` | `cagent::memory_markdown` | File Store |
| `AGENT_BUILD_POSIX_FILE_STORE` | `cagent::posix_file_store` | File Store |

`AGENT_BUILD_MOCK_PROVIDER` 虽有构建入口，官方 Mock 实现尚未交付；
快速开始使用应用自定义 Model。Anthropic 和 STM32 也不能按目录存在推断为可用组件。

例如启用 OpenAI 与 JSONL：

```sh
cmake -S . -B build \
  -DAGENT_BUILD_JSON_CODEC=ON \
  -DAGENT_BUILD_OPENAI_PROVIDER=ON \
  -DAGENT_BUILD_SESSION_JSONL=ON \
  -DAGENT_BUILD_FILE_STORE=ON \
  -DAGENT_BUILD_POSIX_FILE_STORE=ON
cmake --build build
```

平台 Port 不会由普通顶层构建自动启用。SDK 依赖和具体开关见[平台指南](../platforms/index.md)。

## 三个配置时机

| 时机 | 配置 | 影响 |
|---|---|---|
| 编译期 | 源文件裁剪、物理容量、默认 limits | ROM、布局和上限 |
| 初始化/空闲期 | Runtime、Model、Storage、Memory、注册项、limits | 某个实例的依赖与行为 |
| 执行期 | 请求、可选 limits 覆盖、Session ID、user_data | 当前 Turn |

`agent_config_t` 只有系统提示、默认 limits 和 Runtime。它不包含 Transport、
Model、文件路径或 Workspace；后者由对应初始化/绑定接口提供。

## 容量配置一致性

普通 CMake 接受 `CONFIG_AGENT_*` 变量，生成 `agent_build_config.h`。
目标传播 `AGENT_BUILD_CONFIG_HEADER`，让应用与库看到相同的 Workspace 布局。

```sh
cmake -S . -B build \
  -DCONFIG_AGENT_MAX_TOOLS=16 \
  -DCONFIG_AGENT_DEFAULT_MAX_STEPS=6
```

其他构建系统可统一提供 `AGENT_BUILD_CONFIG_HEADER`，或全工程一致的宏定义。
覆盖顺序为显式 `AGENT_*`、配置头中的 `CONFIG_AGENT_*`、内建默认值。
不要只在应用的单个源文件中重定义容量：那不会重建库，也会造成布局不一致。

## 裁剪建议

- 无工具产品：`CONFIG_AGENT_MAX_TOOLS=0`，Core 不因 Tool 引入 JSON reader。
- 无 Skill 或 Context：分别设置槽位为 0；调用相关注册接口会返回 NOT_SUPPORTED。
- 无持久化：不编译 Storage Provider，不绑定 Storage。
- JSONL 与 OpenAI 各自需要完整 codec；关闭 Tool 不会消除它们自己的 JSON 依赖。
- 只有读 Markdown：File Store/Markdown 不要求 JSON。

具体宏、默认值与内存预算见[配置参考](../api/config.md)。
