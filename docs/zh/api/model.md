# Model 接口

公共头：`agent/model.h`。Model wrapper 隔离 Core 与厂商协议，应用配置具体 Provider。

## 类型

| 类型 | 用途与寿命 |
|---|---|
| agent_message_role_t | SYSTEM、USER、ASSISTANT、TOOL 规范角色 |
| agent_message_view_t | 消息正文、Tool call/result 配对；借用到 complete 返回 |
| agent_tool_view_t | 名称、描述、Schema、flags 的模型可见投影 |
| agent_model_request_t | 系统提示、消息数组、工具数组、标识、取消、deadline、token 预算 |
| agent_model_sink_t | text 与 tool_call 同步回调；不允许保存 |
| agent_model_ops_t | 必需 complete，可选 destroy |
| agent_model_workspace_t | wrapper 存储，Provider 工作区独立 |

TOOL 消息的 tool_call_id 必须匹配 ASSISTANT 调用。
Provider 不能让 wire JSON 或第三方 JSON 类型渗入规范请求。

## 包装器与绑定

| 函数 | 行为 |
|---|---|
| agent_model_init | 在调用方 Workspace 初始化 wrapper，复制 Ops、借用 context |
| agent_model_create | allocator 分配 wrapper，失败返回 NULL |
| agent_model_destroy | 销毁未绑定且空闲 wrapper；固定存储不释放 |
| agent_set_model | 空闲借用绑定，应用保留所有权 |
| agent_set_model_owned | 成功才转移 wrapper 所有权 |
| agent_get_model | 获取当前借用 wrapper，非线程安全的配置入口 |

销毁 Agent 前不要销毁仍绑定的 borrowed wrapper。
Provider 状态与 wrapper 是不同对象；其清理由 Provider destroy 契约负责。
同一 Provider 并发使用必须另行设计，当前 OpenAI 实例只允许一个活动调用。
set_model 不接受 NULL 解绑；替换为另一个有效 wrapper 后，应用才可销毁旧 borrowed wrapper。
重复绑定同一 wrapper 返回 EXISTS，不会改变其所有权。

## complete 契约

- 同步返回前完成全部 sink 调用；不能在返回后异步继续投递。
- sink 在驱动上下文串行调用，首个 sink 错误应停止投递并传播。
- text 是 UTF-8 文本块；tool_call 是完整调用和完整参数对象，不是增量片段。
- 输入与 sink/context 不可跨调用保存，不允许与可写 Provider 工作缓冲重叠。
- Provider 遵守 deadline、timeout 和 token 请求预算，但服务器遵守 token 上限仍需验证。
- 当前 sink 没有 usage 通道，不应伪造 token 统计。

自定义 Provider 可以不使用 HTTP/JSON；OpenAI 实例配置见[接入指南](../guides/openai.md)。
