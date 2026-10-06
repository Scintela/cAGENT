# Context 架构

Context 统一协调模型可见输入，各领域保留原始状态和不变量。此模块已实现并通过联合测试，
Run 调度仍待接入；应用只使用注册接口，不包含私有头文件。

```text
Run driver
  -> Session turn transaction
  -> Context prepare
       fixed instructions + Skill selection + Memory snapshots
  -> Context project (each model call)
       dynamic callbacks + Tool views + Session history/current messages
  -> canonical Model request
       system_prompt / messages[] / tools[]
  -> Model complete
  -> release disposable projection
  -> append response facts / execute tools / next projection
```

## 文件与结构体

| 文件 | 职责 |
|---|---|
| `context_registry.c` | 动态贡献与 Memory 选择的共用固定注册表 |
| `context_builder.c` | 同步回调、取消/deadline、sticky sink、完整 UTF-8 校验 |
| `context_projection.c` | turn 快照、跨领域预算、模型输入组装和 LIFO 回收 |
| `context_internal.h` | 私有状态、来源、跳过报告和驱动接口 |

`agent_context_turn_t` 保存所属 Agent、复制的 request/effective limits、取消/deadline、
静态贡献视图、Memory 快照和初始跳过报告。它位于 turn scratch。
`agent_context_projection_t` 是驱动持有、放在 scratch 外的描述符，保存模型请求、mark/end 和报告。
`agent_context_report_t` 是有界的来源名字/错误数组，不包含全文或凭证。

## 驱动契约

1. Run 先在同一 Agent scratch 开启 Session 事务，并进入 ACTIVE。
2. `agent_context_prepare()` 预检必需上界，选择 Skill、读取选中的 Memory。失败回退准备 mark。
3. Run 在调用级投影前预留模型输出/工具参数等需要跨越模型返回的存储。
4. `agent_context_project()` 生成一次 Model 输入；参数 `remaining_tool_calls=0` 时不投影工具。
5. 同步 Model 返回后，`agent_context_release()` 回退调用级 mark，然后 Run 才追加 Session 事实。
6. 后续模型调用复用 turn 快照，刷新动态来源及消息；turn 结束由 Run 清理整个 scratch。

`project()` 校验 Session 事务所属 arena、会话标识和初始输入，避免混用请求。
空 session id 由 Session 归一化为 default，投影沿用这个实际标识。
有效 limits、trace、取消、输出 token 请求上限和剩余 timeout 进入规范化模型请求。

释放必须后进先出；在投影之后追加新 arena 分配会使 release 返回 STATE。
因此模型 sink 需要写入此前预留的响应区域，不能直接在投影尾部追加长期 Session 事实。
`reserve_bytes` 可限制本次投影使用的尾部空间，但不会替 Run 分配或管理响应存储。

## 数据排列与可信来源

系统文本依次为固定指令、选中的 Skill、SOUL 类 Memory、INSTRUCTIONS 动态贡献。
同一类别内部按稳定优先级排列。其他 Memory 快照与 REFERENCE 动态贡献位于消息数组前部，
之后是历史 Session 消息和当前 turn 消息。参考消息不写回 Session。

Tool schema 保持结构化视图，只做可见性选择；执行授权仍在 Tool guard。
Model Provider 只将规范化输入编码为协议，Context 不构建 tools_json/messages_json。
信任分区不是安全沙箱；应用必须控制指令来源并用 Policy/工具授权约束设备副作用。

## 内存和失败边界

长期区保存 Context/Skill 注册表；turn 区保存事务、快照与投影。模块不使用 heap。
required 的声明上界先参与文本预检，必需快照/回调先执行；可选贡献只整段纳入。
动态回调执行顺序与最终呈现顺序明确区分，以保证必需空间和稳定文本顺序。

消息数组和工具描述符在历史回放前分配，当前消息数量与必需参考消息槽位先预留。
Session 只在剩余容量读取完整历史组，保留原有消息配对检查；每组回放加入取消/deadline 检查。
Storage 回调在 Agent 回调保护下执行。同步阻塞 I/O 仍需后端自行提供有界性。

文本总预算为所有贡献正文及保守分隔符开销；历史 payload 使用剩余 scratch。
Memory 的可选快照还会保守预留模型调用描述符、指令缓冲和必需动态输出所需的空间。
这不等于精确 token 统计，也不保证任意 Profile/业务负载必定适配。

动态输出先收集，再按实际长度分配最终 system 文本；某些指令正文因此会暂时存在两份。
峰值包含这种复制与对齐开销。缩小 mark 不回退高水位统计；失败构建也计入峰值。
调用级构建通过局部 arena 描述符分配，只有成功才发布 used；失败不会改变原事务和快照。
Core 的零 heap 不约束外部回调、存储后端或 HTTP/TLS 的分配。

## 验证与未实现范围

契约测试覆盖 Memory 快照、多次投影/释放、动态刷新、跨块 UTF-8、忽略 sink 错误、
必需/可选失败、取消超时、稳定排序、真实 RAM Session 历史和零容量裁剪。
Host 和模拟 IDF 的构建/链接测试验证了关闭 Tool 后不引入 JSON，sanitizer 检查通过。

Run/ReAct、Skill 按需加载、文件 Skill loader、Memory 检索/摘要、模型 token 估算、
公共跳过事件以及真实 MCU 峰值/栈验证不在本次交付范围。
