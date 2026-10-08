# 生命周期与运行接口

公共头：`agent.h`。所有实例操作都要求合法、仍存活的句柄。
以下接口不替应用创建线程、队列或网络连接。

## 生命周期

| 接口 | 作用 | 所有权与失败 |
|---|---|---|
| `agent_init(&agent, &workspace, &config)` | 初始化调用方存储，进入 CONFIGURING | 失败 out 为 NULL；不释放 Workspace |
| `agent_create(&config)` | allocator 申请 Workspace 后初始化 | 失败返回 NULL；需要配对 alloc/free |
| `agent_start(agent)` | 检查状态并进入 READY | 不发起 I/O；允许无 Model |
| `agent_destroy(agent)` | 清理空闲实例及 owned Model | NULL 安全；ACTIVE/回调调用被忽略，不是延迟销毁 |

`agent_init()` 的 Workspace 是完整实例存储，不是临时参数。
已使用的 Workspace 不得在原实例仍存活时重新 init。

## 一次同步运行

```c
agent_error_t agent_run(agent_t* agent,
    const agent_request_t* request, agent_response_t* response);
```

进入前实例应为 READY。请求必须含合法、非空 input；Session ID 空表示默认 Session。
先初始化 `response.output` 与 `response.output_size`，避免未初始化描述符。

| 字段 | 意义 |
|---|---|
| request.session_id / trace_id | 本轮借用的逻辑标识，不是文件路径或凭证 |
| request.input | 不可变 UTF-8 输入，使用期到返回 |
| request.limits | NULL 继承；非 NULL 完整覆盖并在入口复制 |
| request.user_data | 转发给 Context、Tool 和 Policy 的产品上下文 |
| response.status | 执行终态，与函数返回对应 |
| response.session_status | 首个 Session 事务错误，不代表持久化保证 |
| response.delivery_status | 最终文本交付状态 |
| output_written / required | 已交付/完整文本字节数，不含终止符 |
| output_truncated | 输出容量不足，按 UTF-8 边界交付 |
| summary / stats | 本轮事实与累计快照 |

零输出容量允许不交付文本；非零容量须有合法可写指针。
缓冲不足可在执行成功时产生 TRUNCATED 交付状态；不能仅因截断重跑请求。
存储失败、模型失败、工具副作用和输出交付是不同事实。

请求准入成功后进入 ACTIVE；终止路径整理 Session、事件、统计和 scratch 后回到 READY。
取消和失败后不留下公开可恢复 Turn。

## 取消与 limits

| 接口 | 契约 |
|---|---|
| `agent_cancel(agent)` | 请求协作取消；空闲时无操作；非 ISR 安全 |
| `agent_cancel_token_is_set(token)` | 长回调轮询当前借用 token |
| `agent_set_limits(agent, &limits)` | 空闲修改后续 Turn 的默认值，不改变物理容量 |

跨任务取消要注入同步钩子。回调中不重入 Agent；不能用 cancel 代替等待 Run 完成。
详细状态机见[执行架构](../arch/run.md)。
