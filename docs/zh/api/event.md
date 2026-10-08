# Event 与 Stats 接口

公共头：`agent/event.h`。

| 接口 | 契约 |
|---|---|
| agent_set_event_callback | 空闲替换/清除单个观察者；callback 与 user_data 借用 |
| agent_get_stats | 复制累计快照；与其他实例访问串行化 |

事件类型为 TURN_BEGIN、MODEL_BEGIN、MODEL_END、TOOL_BEGIN、TOOL_END、TURN_END。
没有单独 ERROR；对应结束事件的 status 表示失败。
也没有 AGENT_START/END、消息增量事件或流式 Token 事件。

`agent_event_t` 包含 type、单调 timestamp_ms、Session/Trace ID、
工具事件专用 tool_call、相关 status 和当前 summary。
所有引用对象在回调返回后失效，事件不改变执行结果。

观察者在驱动任务上同步执行，不允许重入 Agent，包括直接 cancel。
异步遥测必须复制必要数据；事件不是 Session 的持久化日志。
用法与执行时序见[观测指南](../guides/observability.md)。
