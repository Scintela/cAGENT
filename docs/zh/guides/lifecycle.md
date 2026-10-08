# 生命周期与并发

## 装配一个实例

```text
提供 Runtime 与 Workspace
  → agent_init()                    CONFIGURING
  → 绑定 Model / Storage / Memory
  → 注册 Tool / Skill / Context
  → 设置 Policy 与 Event
  → agent_start()                   READY
  → agent_run()                     ACTIVE
  → 返回                            READY
  → 空闲时修改装配或销毁
```

`agent_start()` 不创建线程、不连接 LLM，也不是“启动一个 Session”。
Session ID 属于请求；一次 `agent_run()` 对应一个用户 Turn，可包含多次模型调用。

允许无 Model 启动以便配置与测试；需要 Model 的运行会在使用点失败。
同一个实例一次只驱动一个 Turn，不支持运行中切换 Model、注册工具或写 Memory。

## 谁拥有资源

| 资源 | 所有者 | 最短有效期 |
|---|---|---|
| `agent_init()` 的 Core Workspace | 应用 | Agent 销毁前 |
| `agent_create()` 的 Core Workspace | Agent 配对 allocator | Agent 销毁时释放 |
| 默认系统提示、Runtime context | 应用 | Agent 使用期 |
| Tool/Skill 名称、内容和 user_data | 应用，注册仅浅拷贝 | 注销或销毁前 |
| `agent_set_model()` 的 wrapper 和 Provider | 应用 | 解除绑定/Agent 销毁前 |
| `agent_set_model_owned()` 的 wrapper | 成功后移交 Agent | Agent 替换/销毁时清理 |
| Storage/Memory context 及内部缓冲 | 应用，Ops 按契约复制 | 使用结束并解除绑定前 |
| Model/Tool/Event 回调中的视图 | 生产方 | 当前回调返回前 |
| 请求输入及 user_data | 调用方 | `agent_run()` 返回前 |
| 最终输出缓冲 | 调用方 | 返回后仍由调用方持有 |

销毁 wrapper 不代表释放任意 Provider 内存。Model 的可选 `ops.destroy` 负责
Provider 自己约定的清理，wrapper 只处理自身存储。固定存储不需要配置 allocator。

## 多任务应用

推荐 UI/语音任务向应用队列投递请求，由一个 Agent 工作任务调用 `agent_run()`。
Core 不创建任务或队列。普通查询、注册、销毁和共享 Store 的读写由应用串行化。

跨任务 `agent_cancel()` 需要 Runtime 的成对 `cancel_sync.enter/leave` 保护。
这是短临界区，不是把整个 Agent 变成线程安全的锁。取消不是 ISR 安全接口。
只有在工作任务返回且所有消费者结束后，才能销毁 Runtime、Provider 或 Workspace。

## 回调纪律

- 回调同步执行在驱动上下文；禁止重入 Agent，包括 Event 回调里直接 cancel。
- Event 只做观测；需要后续处理时复制必要字段并投递到应用队列。
- 不保存 sink、token 或回调视图；延迟使用前必须复制数据。
- 阻塞 SDK 未返回时，Core 不能抢占它；长任务自己轮询取消和绝对 deadline。
- Workspace、Provider 缓冲、输出和输入不得以非法方式重叠。

详见[运行接口](../api/core.md)、[Runtime 契约](../api/runtime.md)和[安全边界](security.md)。
