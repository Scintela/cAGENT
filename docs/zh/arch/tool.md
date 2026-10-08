# Tool 模块架构

同步注册、准入、授权与执行已接入 ReAct 调用和 Session/Event 编排。
公共契约见[Tool 接口](../api/tool.md)，验证记录见[开发日志](../development/tool.md)。
本文是当前 Tool 实现边界，取代旧目标架构中 schema 缓存、descriptor 强制注册和
多 Policy contributor 的描述，不引入暂停确认、自动重试或并行执行。

## 1. 文件和责任

```text
include/agent/
  tool.h                     Tool 定义、回调、公共注册接口
  policy.h                   单个产品授权回调
  types.h                    Tool-call view、sink、limits 等跨模块值类型
  model.h                    Model 消费的 Tool view
src/
  core/lifecycle.c           初始化 workspace；预留 registry 与 scratch
  tool/
    tool_internal.h          私有 registry、投影、调用入口和执行事实
    tool_registry.c          槽位维护、借用校验、枚举、可见投影
    tool_schema.c            文本/JSON 对象准入，不执行 JSON Schema
    tool_guard.c             取消/时限、validate、Policy、handler、输出
  policy/policy_chain.c       单 Policy 设置与 fail-closed 决策
  run/                       迭代、预算、Session/Event、统计
codecs/json/
  reader.c                   严格 JSON/UTF-8 与无 token 的唯一键对象校验
  writer.c                   Provider 格式化；Tool 本身不依赖
providers/model/openai/       规范 Tool view -> OpenAI tools JSON
tests/tool/                  运行契约和容量 Profile
tests/build/tool/            Host/模拟 IDF 构建与实际链接
```

Tool 不包含 SDK、HTTP、文件格式、模型 wire protocol 或具体智能家居设备操作。
产品 handler 可调用其设备、网络或文件后端，内存行为和幂等性由实现者负责。
Core Tool 自身不申请 heap；caller workspace 可以由应用静态提供，也可在初始化前由
应用分配。可选 `agent_create()` 的整块分配不等于每次 Tool 执行分配 heap。

## 2. 内存和寿命

```text
Agent Workspace
  Persistent
    agent_t
    agent_tool_registry_t
      count
      entries[AGENT_MAX_TOOLS]    复制的定义；字符串和 user_data 仍为外部借用
  Turn scratch
    可见 agent_tool_view_t[]
    Run 保留的 Tool-call 参数/ID
    单次输出缓冲 + 执行事实

Application
  静态文本 / addon 持有的 schema
  设备 callback 状态
  Model Provider 请求/响应缓冲
```

registry 在 init 时分配并清零；零槽位时 registry 指针为 NULL，没有实际 registry 分配。
注册先校验全部字段再写入一个槽位，失败不增加 count。注销压紧数组并清空尾槽，保留
其他项顺序。没有 owned Tool 模式、深拷贝池或隐藏 realloc。

投影仅复制可见定义中的 name/description/schema/flags，借用文本并预检输出数组容量。
容量不足不写半个数组，返回 count=0/CAPACITY。registry 文本覆盖、整数乘法溢出和
数组/count 重叠均拒绝。视图在注册表或其借用文本变化前有效；Run 必须在本轮使用期间
冻结注册表，不能跨注销/重配置保留指向条目的指针。

## 3. 请求与执行流

```text
应用注册 Tool                         已实现
  -> idle/reentry 检查
  -> 元数据、flag、schema 准入
  -> Persistent registry
  -> 可见 Tool view 投影

Run -> Model request -> Provider      已接入
  -> tools JSON -> HTTP -> 模型
  -> 完整 Tool-call sink -> Run 复制 ID/name/arguments
  -> Run 预留结果空间、传入剩余调用预算

agent_tool_invoke (私有)               已实现
  -> ACTIVE/缓冲互斥/输出空间预检
  -> lookup + visibility + IDs
  -> effective deadline / cancel / remaining budget
  -> arguments JSON 对象校验
  -> 可选纯 validate
  -> 单个 Policy 明确 ALLOW（否则拒绝）
  -> 再检查 cancel/deadline
  -> handler 一次 -> bounded sticky sink
  -> 输出文本校验 + 返回执行事实

Run 消费执行事实                       已接入
  -> handler_called 则扣减预算、记录调用事实
  -> 配对 Tool-call/result、事件和统计
  -> 下一次 Model 迭代或终止 turn
```

运行中的 Token/Tool-call 数组所有权、唯一 call ID、完整 assistant-call/result 配对、
失败持久化及事件顺序由 Run/Session 负责，Tool 不擅自追加历史。没有公开的 bypass
执行函数；应用不应包含 `src/tool/tool_internal.h` 或手动更改 Agent state。

## 4. 三个相互独立的检查

1. **结构准入**：长度、完整 JSON 对象、UTF-8、转义、数字文法、深度和唯一键。
2. **业务语义**：validate/handler 验证字段类型、值域、未知字段、设备前置条件。
3. **授权**：Policy 根据产品身份、来源、设备状态等决定；schema/READ_ONLY 不授予权限。

注册 schema 和调用 arguments 共用对象准入辅助，但分别采用 schema/arguments 字节上限。
对象准入不创建 token 数组。先完成严格语法验证，再比较各对象成员的解码键；包含
转义等价和嵌套重复键，NUL 键拒绝。原有通用 JSON parse 的重复键行为不被暗中改变。

为避免最大 token 数组占据额外 scratch，唯一键检查使用有界重扫，最坏字节工作量为
二次量级，栈随深度增长而非随成员数增长。它不是常数时间算法：产品显著扩大输入上限时，
须测量 CPU/任务栈，再评估 token 或索引方案。深度 32 是语法硬上限，不是所有 MCU
任务栈都足够的承诺；真实栈峰值须用目标工具链和配置测量。

## 5. 失败事实与输出

私有 `agent_tool_execution_t` 不与公共 `agent_response_t` 混用：

| 字段 | 含义 |
|---|---|
| `status` | 调用最终状态，反映准入、取消/时限、输出和 handler 结果 |
| `handler_called` | 在调用 handler 前置 true；表示可能已有设备副作用 |
| `handler_status` | handler 的归一化返回值；未调用时是 NOT_SUPPORTED，仅在 called=true 时解释 |
| `output_status` | sticky sink 或最终 UTF-8/NUL 检查结果 |
| `output` | 借用调用者目标缓冲；失败时可能是部分诊断文本，不能当作成功结果 |

安全前检发现参数别名、非法指针组合或重入时，返回错误且不写结果。完成前检后的
失败初始化 empty output/called=false。handler 返回后，终态优先级为：当前取消/超时，
再输出错误，再 handler 错误；三个结果字段仍保留自己的事实。
validate/Policy 的错误会停止后续执行，不反复调用已失败阶段。

输出空间在副作用前按完整单次上限预留。write 原子地接受整个块或拒绝，不静默截断，
首错后不再接收；最后允许跨块 UTF-8 组合，但不允许整体非法字节。失败的部分文本仅供
编排形成明确错误结果，不能仅凭其内容判断设备是否执行成功，也不能自动再试一次。

## 6. 生命周期与控制边界

- 注册、注销、启停和 Policy 设置只允许 CONFIGURING/READY；ACTIVE 返回 BUSY。
- 查询/枚举只在单 driver 回调外进行，枚举也受 callback guard 保护。
- validate、Policy、handler 和该调用中的 Runtime 检查处于重入保护内；业务回调不重入
  Agent，协作式 cancel 是例外。callback guard 不提供多任务序列化。
- 工具时限从调用阶段开始计算，覆盖参数验证、Policy 和 handler；整体 deadline 由 Run
  传入，取更紧者。每个阶段边界及每次 write 检查，阻塞 callback 返回后才能观察超时。
- Policy 缺失/无效/CONFIRM 与 REQUIRES_CONFIRM 均 fail closed；没有等待 UI 的隐藏状态。
- Tool 不维护 turn 全局调用计数；Run 必须根据 handler_called 消耗预算，即使 handler
  返回失败也计入。Tool 只执行一次，限额不代表并行数或 registry 大小。

## 7. 构建边界

Core Tool 启用时链接 `cagent_json_reader`，不需要完整 writer。显式启用
`AGENT_BUILD_JSON_CODEC` 时，`cagent_json_jsmn` 提供 writer 并依赖同一 reader。
OpenAI/JSONL 复用该目标，不再重复编译 reader。ESP-IDF 组件用同样条件收集源文件。

`AGENT_MAX_TOOLS=0` 时不因 Tool 编译 JSON reader；其他 Provider 独立需要 codec 时
仍可启用。这表示条件依赖，不表示“启用 Tool 的 Core 完全没有 JSON 语法依赖”。
公共头没有第三方 JSON 类型，jsmn 的内部符号继续保持 translation-unit 隔离。
