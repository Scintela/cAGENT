# 基础类型

公共头：`agent/types.h`。这里放跨模块句柄与值类型，不是所有领域结构体的集合。

## 文本与 sink

```c
typedef struct {
    const char* data;
    size_t size;
} agent_string_view_t;
```

size 是字节数，不含隐含终止符；非零长度必须有合法指针。
`agent_string_view(data, size)` 只构造视图，不验证编码或复制。
`AGENT_SV_LITERAL("text")` 用于聚合初始化，表达式使用构造函数。
`agent_string_view_is_empty()` 只检查长度，不验证指针。

不要将不保证终止的视图直接传给 strlen、strcmp 或 printf 的 %s。
字符串可能借用 RAM、常量或 Provider 缓冲，其寿命由具体接口决定。

`agent_text_sink_t` 包含 write 与 context，同步消费一段视图；
生产者检查每次返回值，不能保存 sink 或在调用结束后写入。

## Tool Call

`agent_tool_call_view_t` 包含调用 id、工具 name、完整 arguments_json 对象。
它描述模型产生的调用，不是注册 Tool 的元数据。
Model、Tool、Session 都使用此值类型；回调中的正文仅在回调期间有效。

## Limits

| 字段 | 单位与零值 |
|---|---|
| max_steps | 模型迭代次数；必须非零 |
| timeout_ms | 整体毫秒预算；0 无此限制 |
| per_model_timeout_ms | 每次模型调用毫秒预算；0 仅整体 deadline |
| per_tool_timeout_ms | 每次工具毫秒预算；0 仅整体 deadline |
| max_tool_calls | handler 尝试次数；0 禁止工具 |
| max_output_tokens | 每次模型请求的 token 预算；0 未指定 |
| max_history_turns | 历史完整组数；0 不投影历史 |

`AGENT_LIMITS_DEFAULT` 来自构建默认宏。limits 是行为预算，不是槽位分配：
12 个注册工具与最多执行 4 次工具是独立限制。

## 请求、结果与统计

`agent_request_t` 是一次用户请求；`agent_response_t` 分开执行、Session 与文本交付结果。
`agent_run_summary_t` 记录模型/工具次数、失败/拒绝、最终结果、耗时和 scratch 峰值。
`agent_stats_t` 是累计快照，计数饱和而不是回绕，不包含模型 usage。

字段及使用见[运行参考](core.md)和[事件参考](event.md)。
