# Runtime 接口

公共头：`agent/runtime.h`。Runtime 是应用注入的基础服务表，不是操作系统或调度器。

| 字段/类型 | 必需性 | 契约 |
|---|---|---|
| now_ms / clock_context | 时钟必需 | 单调毫秒，Core/Provider 使用同一时间基准 |
| agent_allocator_t | 可选 | alloc/free 成对；固定存储路径可以不提供 |
| agent_sync_t cancel_sync | 可选 | enter/leave 成对，保护短取消状态访问 |
| log / log_context | 可选 | 视图只在日志回调期间有效，禁止泄露凭证 |

服务表复制，所有 context 仍由应用持有到消费者结束。Runtime 一般在初始化时确定；
没有公共运行中替换 Runtime 的接口。

不要求线程、互斥锁、事件、文件系统和网络都进入一个 OSAL 大表。
Core 在应用任务中同步执行；Transport/File Store 分别有自己的能力契约。

跨任务取消时注入同步，而不是只把布尔值标为 volatile。
同步钩子不能执行阻塞 I/O，普通 Agent API 仍须外部串行化。

平台 builder 不保证都安装相同的可选能力：
ESP-IDF/OpenVela 最小 builder 提供时钟；RT-Thread builder 还有明确的状态和定时器生命周期。
详情见[平台说明](../platforms/index.md)。
