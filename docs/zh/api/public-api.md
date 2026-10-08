# API 总览

本节说明当前可调用接口、结构体与行为契约。它不是 V1 迁移草案或未来声明清单。
所有接口使用 `agent_` 前缀，返回状态为 `agent_error_t`，只有 AGENT_OK 成功。

公共头以当前源码为准。按同一源码版本和容量配置编译应用、Core、Provider；
不承诺不同版本、平台或 Profile 的二进制 ABI。

## 按任务查找

| 任务 | 公共头 | 参考 |
|---|---|---|
| 初始化、启动、运行、取消 | `agent.h` | [生命周期与运行](core.md) |
| 容量、Workspace、默认配置 | `agent/config.h` | [配置](config.md) |
| 句柄、文本、limits、请求与结果 | `agent/types.h` | [基础类型](types.md) |
| 错误分类与诊断名称 | `agent/error.h` | [错误码](error.md) |
| Tool 注册及执行回调 | `agent/tool.h` | [Tool](tool.md) |
| Tool 授权 | `agent/policy.h` | [Policy](policy.md) |
| 静态指令贡献 | `agent/skill.h` | [Skill](skill.md) |
| 动态资料及 Memory 选择 | `agent/context.h` | [Context](context.md) |
| 会话 Storage 与查询 | `agent/session.h` | [Session](session.md) |
| 长期文档的整文操作 | `agent/memory.h` | [Memory](memory.md) |
| 事件与累计统计 | `agent/event.h` | [Event](event.md) |
| 自定义模型实现与绑定 | `agent/model.h` | [Model](model.md) |
| 时钟、分配、日志和取消同步 | `agent/runtime.h` | [Runtime](runtime.md) |
| 同步 HTTP 后端 | `agent/transport.h` | [Transport](transport.md) |
| 可选 RAM、JSONL、File Store、Markdown | 各 Provider 独立头 | [Storage](storage.md) |

`agent.h` 聚合常用入口，不自动引入平台 SDK、具体 Model、Memory、Skill 或 Context 的全部接口。
需要它们时显式包含相应头。没有公共 `run.h`、`plugin.h`、`agent_plan()` 或 turn step/resume API。

## 通用规则

- 视图是 `data + size`，不是必须 NUL 终止的字符串。
- 注册/绑定常为浅拷贝；不得根据“复制结构体”推断正文或 context 被复制。
- 普通操作要求驱动任务串行访问；ACTIVE 和回调中不能修改实例。
- 回调视图仅在当前回调有效，sink 不可保存或异步调用。
- 调用者管理 Core 外的 Provider、HTTP/TLS、Storage 缓冲和任务栈。
- 执行失败不保证外部操作未发生，持久化失败不保证数据未发布。
- 具体支持边界见[模块地图](../arch/module-map.md)和[平台概览](../platforms/index.md)。

从零接入先看[快速开始](../getting-started/quickstart.md)，不要从 Ops 表开始组装整个应用。
