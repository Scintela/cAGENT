# Policy 接口

公共头：`agent/policy.h`。当前实现是单个应用回调，不是通用多策略框架。

| 类型/值 | 含义 |
|---|---|
| agent_policy_request_t | 注册 Tool、执行上下文、MODEL 来源；只读回调视图 |
| AGENT_POLICY_DENY | 拒绝，零初始化默认 |
| AGENT_POLICY_ALLOW | 允许，但不能绕过工具自身确认标志 |
| AGENT_POLICY_CONFIRM | 同步模式失败关闭，不保留等待状态 |

`agent_set_policy_callback(agent, callback, user_data)` 仅空闲调用。
NULL 或无效决策拒绝所有模型工具调用。user_data 借用到替换/销毁。
回调不应产生设备副作用，不允许 Agent 重入。

名称、Schema、group/category 或 READ_ONLY 不构成用户授权。
应用应基于 request_user_data 中的可信身份与产品权限明确判断。
示例见[Tool 指南](../guides/tools.md)。
