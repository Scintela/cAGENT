# 事件、统计与取消

## 注册观察者

```c
#include <agent.h>
#include <stdio.h>

static void on_event(void* data, const agent_event_t* event)
{
    (void)data;
    printf("event=%d status=%s models=%u tools=%u\n",
        (int)event->type, agent_error_str(event->status),
        (unsigned int)event->summary.model_calls,
        (unsigned int)event->summary.tool_calls);
}
```

空闲时调用 `agent_set_event_callback(agent, on_event, NULL)`。
真实产品避免在回调里阻塞打印；若需异步上报，复制有限字段并投递应用队列。

## 一次 Turn 的边界

```text
TURN_BEGIN
  MODEL_BEGIN → MODEL_END
  TOOL_BEGIN  → TOOL_END       可为零个或多个
  MODEL_BEGIN → MODEL_END     可继续迭代
TURN_END
```

MODEL_END/TOOL_END 携带失败状态；TURN_END 表示终态，不需要再添加泛化 ERROR
才能知道失败。Tool 开始/结束描述实际 handler 调用；调用在前置守卫被拒绝时，
不要假定存在 handler 边界事件。事件不等于可恢复的持久日志。

未通过请求准入的错误不属于“已开始的 Turn”，不要期待它有 TURN_BEGIN/END。
一旦 Turn 已开始，正常终止与失败/取消都通过终止路径收尾。

## 统计与输出

`agent_get_stats()` 获取累计快照，需要与其他访问串行化。
`response.summary` 是本轮事实；`response.stats` 是运行后的累计快照。
目前统计调用次数、耗时和 scratch 峰值，不含模型 usage token 数。

`agent_run()` 返回执行状态，`delivery_status` 单独表示最终文本交付是否截断。
输出小了只影响交付，不会使工具自动重执行。

## 协作取消

工作任务执行 Run，其他任务通过已配置同步钩子的 Runtime 调用 `agent_cancel()`。
Core、Provider 和长时间 Tool 检查 borrowed token 与 deadline。
阻塞 SDK 未返回时可能无法立即取消；不要据此释放仍在使用的缓冲。

回调中不要直接重入 cancel；将取消意图交给外部应用控制流程。
接口字段详见[事件参考](../api/event.md)。
