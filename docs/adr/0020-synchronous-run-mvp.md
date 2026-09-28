# ADR 0020: 同步运行 MVP 的公开边界

- 状态：已采纳（公开接口范围）；完整执行链待实现
- 日期：2026-09-28

## 决定

首版只公开 `agent_run()` 作为一次用户请求的同步入口。`agent/run.h` 暂时只保留
`agent_cancel()` 与供 Model/Tool 查询的 `agent_cancel_token_is_set()`；`agent.h`
包含 `run.h`，普通应用无需额外包含头文件。`agent_turn_t`、`begin/step/resume/end`、
step 结果、运行阶段枚举和确认 nonce 不进入 MVP 公共契约。内部可以使用私有状态机，
但不能把内部阶段误称为已实现的公开步进能力。

确认能力同样延期：`AGENT_POLICY_CONFIRM` 或 `AGENT_TOOL_REQUIRES_CONFIRM` 在同步
`agent_run()` 中必须拒绝执行，绝不自动批准，也不留待恢复的隐藏 turn。拒绝可计入
`summary.tool_denied`；不发出暗示存在等待状态的 `AGENT_EVENT_CONFIRMATION`。
Policy 的 `DENY` 仍不可被其他路径覆盖。当前 Tool guard 和 `agent_run()` 的完整
执行链尚未实现，以上是后续实现的强制语义，不是已通过的运行验证。

取消为协作式：空闲调用 `agent_cancel()` 是无操作；活动运行需要在配对的
`runtime.cancel_sync` 保护下发布与清除活动 token，Model/Tool/Transport 在安全点
轮询它。跨任务使用必须提供同步回调，不保证中断正在阻塞的网络 SDK，也不支持 ISR。
回调内不得重入 Agent。设备副作用一旦发生，不因取消或超时而自动回滚。

Live Event 仍保留 TURN/MODEL/TOOL 的边界与 `status`，本轮摘要由
`agent_response_t.summary` 承载；流式文本若需要对应用交付，应另立有界 sink，
不通过增加 step API 或复用 Event 回调来伪装异步执行。

## 理由与影响

普通智能家居查询和设备控制可在应用工作任务中调用同步 `agent_run()`。暂停确认
需要持有借用输入、参数与工作区，定义超时、取消、重入及恢复后的授权校验；在这些
契约及测试未完成前公开接口，增加误用面而不增加可用能力。保留小型 `run.h` 是为
Model/Tool 取消轮询提供稳定依赖边界，并不意味着首版提供可暂停的 turn。

ADR 0012、0015 和 `docs/api/public-api.md` 中的 step/resume 方案作为后续候选，
不再代表 MVP 公开 API。若真实产品需要异步人工确认，须另行裁决其状态与内存模型、
发布可测试的恢复契约；不能仅恢复旧声明。

## 验证要求

- C99/C++11 头文件及现有调用方不依赖已移除的公开 turn 类型；
- 空闲 cancel 不影响后续运行，活动 token 可由 Provider/Tool 观察；
- Tool/Policy 要求确认时不调用 handler，且不留下待恢复的内部状态；
- `agent_run()` 完成后所有借用视图失效，事件边界与统计只反映实际执行。
