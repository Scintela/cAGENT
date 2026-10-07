# RT-Thread Runtime

公共头：`agent_rtthread_runtime.h`。状态由应用零初始化并固定地址持有：

```c
static agent_rtthread_runtime_t runtime_state;
agent_runtime_t runtime;
agent_error_t error = agent_port_rtthread_runtime_init(&runtime, &runtime_state, false);
```

检查成功后，将 `runtime` 复制到 `agent_config_t.runtime` 和需要时钟的 Provider
配置。`false` 不安装 allocator；caller-owned workspace 不需要 heap。
`true` 仅在 `RT_USING_HEAP` 时提供 `rt_malloc/rt_free`，否则返回 NOT_SUPPORTED。
失败不发布 `out`。

对 `rt_tick_get()` 的 32-bit tick 做差分扩展，计算 64-bit 毫秒。调用方持有的静态
周期 timer 每四分之一个 tick 回绕周期采样，避免 Agent 长期空闲后丢失回绕。
时钟锁和取消锁使用不同的 `rt_spin_lock_irqsave`，不把 I/O 放入临界区。
Runtime 不创建工作线程、队列，也不自动安装日志回调。

- 内核 timer 初始化后、任务上下文中初始化；生命周期独立于 Agent。
- 不允许运行中重设 tick。BSP tickless 必须补偿休眠 tick；否则 deadline 只反映
  活动 tick，不代表墙钟时间。采样不能被延迟整个 tick 回绕周期。
- `agent_cancel()` 仍是任务 API，不因为 irqsave 而变成 ISR-safe。
- 销毁全部消费者并确保无时钟/取消调用后才能 deinit。SMP/all-soft 配置还需 BSP
  确保没有正在派发的采样回调；detach 不是 callback join。常驻固件可保留静态
  Runtime 直到关机。
- deinit 不清空已发布的服务表副本；之后不得继续使用它们。

测试覆盖 100/1000/1024 Hz、多次回绕、初始化失败回滚、重复初始化、取消锁配对、
detach 失败和 heap 开关；尚未验证 SMP 调度和 BSP 低功耗。
