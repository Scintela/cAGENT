# 同步执行架构

主驱动为 `src/run/react_loop.c`，应用入口为 `agent_run()`。

## 三个时间层次

- Agent：注册表和绑定的寿命，可运行多个 Turn。
- Turn：一次用户请求、有效 limits、取消 token 与当前消息事实。
- 模型迭代：一次 complete，可返回 final 或多个完整 Tool Call。

Session 是跨 Turn 的逻辑对话，不通过 agent_start 创建。

## 输入与事实的寿命

每轮准备保留 Skill/Memory 的稳定快照。每次模型调用重新投影动态状态、工具及消息，
临时投影在调用后释放；模型 sink 输出先复制，再保留进当前 Turn。
不能直接保存 Provider 的 decoded_buffer 指针作为后续历史。

handler 结果配对到 Tool Call ID；失败路径也要维护模型可理解的调用/结果关系。
模型 final 存在与文本是否交付成功是两个事实。

## 终态处理

正常返回、模型错误、容量耗尽、取消、超时都进入统一收尾路径：
终结 Storage 事务、交付已有合法 final、统计、结束事件、撤销活动 token、复位 scratch。
finish 消费事务，即使持久化失败也不留下公开可恢复的活动 Turn。

超时/取消不会撤销已执行设备动作。Core 不自动重试副作用 handler。

## 测试入口

`tests/run/contract.c` 覆盖确定性 Model/Tool、Storage 故障、预算、交付、取消和重入；
`tests/run/openai.c` 接入真实 OpenAI Provider 与假 HTTP。
这些测试验证同步软件链，不验证真实远端服务或设备动作安全。
