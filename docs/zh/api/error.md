# 错误码

公共头：`agent/error.h`。采用扁平 enum：通用错误说明可跨模块共享的失败，
模块错误保留具体语义。只有 AGENT_OK 成功，错误均为负值。

| 名称 | 数值 | 含义 |
|---|---:|---|
| AGENT_OK | 0 | 成功 |
| AGENT_ERROR | -1 | 无更精确分类的外部失败 |
| AGENT_ERROR_NOMEM | -2 | allocator/Provider Heap 分配失败 |
| AGENT_ERROR_INVALID | -3 | 参数、配置或本地数据非法 |
| AGENT_ERROR_STATE | -4 | 生命周期状态不允许操作 |
| AGENT_ERROR_BUSY | -5 | ACTIVE 或回调重入妨碍操作 |
| AGENT_ERROR_LIMIT | -6 | 行为或有界对象限制 |
| AGENT_ERROR_TIMEOUT | -7 | 有效 deadline 到期 |
| AGENT_ERROR_CANCELLED | -8 | 观察到协作取消 |
| AGENT_ERROR_NOT_FOUND | -9 | 对象不存在 |
| AGENT_ERROR_EXISTS | -10 | 名称重复 |
| AGENT_ERROR_NOT_SUPPORTED | -11 | 当前构建/后端不支持 |
| AGENT_ERROR_IO | -12 | I/O 未成功完成 |
| AGENT_ERROR_AUTH | -13 | 远端凭证/授权拒绝 |
| AGENT_ERROR_TRUNCATED | -14 | 最终文本交付不完整 |
| AGENT_ERROR_PARSE | -15 | 非模型专属输入/格式解析失败 |
| AGENT_ERROR_CAPACITY | -16 | 固定 Workspace、池或 scratch 耗尽 |
| AGENT_ERROR_CONTEXT_OVERFLOW | -32 | Context 文本投影超过限制 |
| AGENT_ERROR_MODEL_FAILED | -48 | 模型失败且无更精确分类 |
| AGENT_ERROR_MODEL_PARSE | -49 | 模型响应无法解析 |
| AGENT_ERROR_MODEL_RATE_LIMIT | -50 | 模型服务限流/配额拒绝 |
| AGENT_ERROR_MODEL_UNAVAILABLE | -51 | 模型服务暂不可用 |
| AGENT_ERROR_POLICY_DENIED | -64 | 授权/确认拒绝执行 |
| AGENT_ERROR_TOOL_ARGUMENT | -80 | 模型工具参数非法 |
| AGENT_ERROR_TOOL_FAILED | -81 | 工具失败且无更精确分类 |

`agent_error_str(code)` 返回静态诊断名称，永不返回 NULL，不需要释放。
使用 `status == AGENT_OK` 或 `status != AGENT_OK`；没有额外成功/失败 helper。

错误码没有独立 source 字段，也没有 DNS/TLS/HTTP 各阶段的完整细分。
必要时由应用在 Provider/Port 边界记录脱敏诊断。
执行结果还须结合 delivery_status、session_status 或 Memory change，
错误码单独不能证明外部动作/持久化未发生。排查见[故障指南](../guides/troubleshooting.md)。
