# 模块与目录地图

此地图描述当前目录职责；空目录或骨架文件不代表已交付能力。

| 路径 | 职责 | 开发者入口 |
|---|---|---|
| include/agent.h | 生命周期、Run 和常用应用入口 | [Core 参考](../api/core.md) |
| include/agent/ | 平台无关的领域与扩展契约 | [API 总览](../api/public-api.md) |
| src/core/ | Workspace、Arena、事件、状态与错误 | [内存](memory.md) |
| src/run/ | 同步 ReAct 与协作取消 | [执行链](run.md) |
| src/tool/、src/policy/ | 工具注册、准入、授权与执行 | [Tool](tool.md) |
| src/skill/ | 有界可信指令注册表 | [Skill](skill.md) |
| src/context/ | 跨领域模型输入投影 | [Context](context.md) |
| src/session/ | 当前 Turn 与 Storage 转发/历史投影 | [Session](../guides/session.md) |
| src/memory/ | 长期内容领域操作与安全边界 | [Memory](../guides/memory.md) |
| src/model/ | Model wrapper 和绑定，不含厂商协议 | [Model](../api/model.md) |
| src/runtime/、src/transport/ | 通用服务/HTTP 契约验证与转发 | [Runtime](../api/runtime.md)、[HTTP](../api/transport.md) |
| providers/model/openai/ | 非流式 Chat Completions | [OpenAI](../guides/openai.md) |
| providers/storage/ram/、jsonl/ | Session 具体存储 | [Storage](../api/storage.md) |
| providers/storage/files/ | 共享字节文件接口 | [文件 I/O](../api/storage.md) |
| providers/memory/markdown/ | 四类 Markdown 文档映射 | [Memory](../guides/memory.md) |
| codecs/json/ | 私有 reader/writer | [构建](../getting-started/build.md) |
| ports/ | Runtime、HTTP、文件系统适配 | [平台](../platforms/index.md) |
| tests/ | Host 契约、故障注入与构建矩阵 | [验证](../contributing/testing.md) |
| docs/zh/ | 开发者手册及维护资料 | [快速开始](../getting-started/quickstart.md) |

## 不要合并这些职责

通用 Codec 识别 JSON 语法；JSONL Provider 决定 Turn 记录格式。
File Store 操作字节；Session bridge 决定会话文件命名与批量管理。
Context 协调来源；Memory 与 Skill 决定文档/指令含义。
Port 适配 SDK；src/runtime 与 src/transport 保留平台无关契约。

## 尚未交付

官方 Mock、Anthropic、Host Runtime/HTTP、STM32、Skill 文件加载器和内置 Tool
尚无完整正式实现。自动 Session→Memory 提取、异步确认和流式模型也是未来能力；
当前主干不依赖它们才能运行。
