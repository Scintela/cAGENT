# 故障排查

先记录执行状态、Session 状态、交付状态和本轮摘要，再判断是否可以重试。
不要仅看到一个非零错误码就重新执行整个请求。

| 现象/错误 | 首先检查 | 处理 |
|---|---|---|
| init 返回 INVALID | now_ms、allocator/sync 是否成对、max_steps | 修正初始化配置 |
| 编译 Workspace 容量检查失败 | 槽位、scratch、CORE_WORKSPACE_BYTES 同一配置 | 调整总预算并全量重建 |
| CAPACITY | registry 槽位、scratch、Provider 工作缓冲、JSON token 数 | 定位耗尽的域，不只扩大 Core |
| CONTEXT_OVERFLOW | 必需贡献声明、Skill 文本、分隔开销 | 减少贡献或提高文本预算 |
| LIMIT | 步数/Tool 调用、单对象字节、消息描述符 | 分清行为限制与输入准入 |
| BUSY | ACTIVE 时修改/查询、回调重入 | 等 Run 返回，串行化访问 |
| POLICY_DENIED | 未装 Policy、CONFIRM、隐藏/禁用工具 | 检查明确授权，不默认放行 |
| AUTH | Bearer、远端权限、平台 TLS 配置 | 检查安全配置，不打印凭证 |
| MODEL_PARSE | 响应协议、finish_reason、消息字段 | 用脱敏响应复现 Provider 测试 |
| MODEL_RATE_LIMIT | 远端配额或速率 | 应用有界退避，不自动重跑副作用 |
| TIMEOUT/CANCELLED 延迟出现 | SDK 阻塞 DNS/TLS/读写 | 调整 SDK 超时并保持状态活到返回 |
| 文本不足 | delivery_status、output_required | 扩大后续输出缓冲；不能通过重跑交付已执行动作 |
| 历史没进入模型 | max_history_turns 默认 0、Storage、历史完整组 | 显式开启窗口并核对 ID |
| 文件删除/替换报错 | published/change、目录 sync 能力 | 先读回事实，避免盲目重试 |

错误参考见[错误码](../api/error.md)，预算关系见[内存模型](../arch/memory.md)。
模型错误响应诊断和详细容量来源目前没有完整公共结构化通道；
应用可在自己的 Provider/Port 边界记录脱敏信息，但不要改写公共错误语义。

## 复现问题

1. 固定源码提交与统一构建配置。
2. 用确定性 Model/Transport 缩小到一个请求。
3. 保留必要的请求规模、错误、summary 与配置，删除凭证和私人内容。
4. 重跑对应[契约测试](../contributing/testing.md)。
5. 平台问题附 SDK/BSP、TLS 配置、文件系统和任务栈信息。
