# Memory 接口

公共头：`agent/memory.h`。独立的长期内容领域契约，不是 Session 缓存或向量检索 API。

| 类型 | 作用 |
|---|---|
| agent_memory_kind_t | SOUL、USER、FACTS、NOTE |
| agent_memory_key_t | kind + id；仅 NOTE 必须带逻辑 id |
| agent_memory_change_t | UNCHANGED、APPLIED、UNKNOWN 的变更事实 |
| agent_memory_ops_t | 必需 read，可选 replace/forget |
| agent_memory_t | Ops 值 + 借用 context |

## 函数

| 函数 | 行为 |
|---|---|
| agent_set_memory | 空闲绑定/替换；NULL 解绑，无隐含 Provider 销毁 |
| agent_memory_read | 空闲整文读取到 caller buffer，返回 NUL 终止视图 |
| agent_memory_replace | 可信应用整文替换；SOUL 拒绝 |
| agent_memory_forget | 可信应用整文删除；SOUL 拒绝，不是安全擦除 |

read 的 capacity 包含终止符空间，max_bytes 为正文上限，超限不静默截断。
输出视图由调用方缓冲持有，不借用后端文件缓存。

replace/forget 的返回值与 change 必须一起看：失败可能 APPLIED 或 UNKNOWN，
不能按错误自动重试。API 不推断模型授权，也不在运行中的 Tool 回调内支持重入。

绑定 Memory 与选入 Context 是两件事，使用
`agent_register_memory_context()` 指定文档。Markdown 目录与初始化见
[Memory 指南](../guides/memory.md)、[Storage 参考](storage.md)。
