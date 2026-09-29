# ADR 0009: 公共 Types 边界与数据类型归属

- 状态：提案
- 日期：2026-09-24

本文关于公开 `agent_turn_t` 的讨论已由 ADR 0020 暂缓；MVP 的 `types.h` 不声明它。

## 背景

cAgentV2 面向 ESP-IDF、openvela、RT-Thread 与 Host 的同一套 C API。公共头文件既要让
Model、Tool、Context、Run、Event、Session 和 Runtime 共享必要的值类型，又不能把某个
领域模块的描述符扩散为所有调用者的基础依赖。

过度集中的 `types.h` 会让所有模块看到与其无关的领域概念；过度拆分为 `view.h`、
`result.h`、`limits.h` 等许多小头又会增加 include 链、循环依赖风险和 MCU 应用的理解成本。
本 ADR 为 `include/agent/types.h` 规定最小且稳定的边界，并定义类型应迁移到领域头的判断
标准。

## 决定

`types.h` 是**跨模块值类型与不透明句柄的基础层**，不是所有公共 struct 的收容处。它只
包含满足下列至少一项的类型：

1. 两个或以上独立领域模块直接交换该值；
2. 它是 Core workspace、借用期或执行结果的基础契约；
3. 将其移入某个领域头会迫使不相关模块产生反向或循环依赖。

领域专属的 Provider 输入、注册描述符、回调和状态机类型必须留在对应模块头中。公共头
应直接 include 其实际使用的领域头，不能依赖应用恰好先 include 其他头文件。

### `types.h` 保留项

| 类型 | 保留理由 |
|------|----------|
| `agent_t` | 所有注册、生命周期、Session、Event 和 Run API 共享的 Core 不透明句柄。 |
| `agent_turn_t` | Run 公开的短生命周期 token；前置声明放在基础层不会引入 Run 定义。 |
| `agent_cancel_token_t` | Model、Tool、Context 与 Transport 都只需借用其不透明指针。 |
| `agent_string_view_t`、`AGENT_SV_LITERAL`、`agent_string_view()` | 全部无分配文本输入/输出的统一表示。 |
| `agent_text_sink_t` | Model、Tool、Context 和内部投影共享的有界文本输出契约。 |
| `agent_tool_call_view_t` | Model 输出、Tool 输入、Run confirmation 与 Event 观察共同交换的调用事实。 |
| `agent_limits_t` | Config 默认值、Run override、Model 与 Tool deadline/budget 共同引用的执行限制。 |
| `agent_run_summary_t`、`agent_stats_t` | Run response 与 Event snapshot 共享的只读执行事实。 |
| `agent_request_t`、`agent_response_t` | 同步 `agent_run()`、Turn、Context 和 Event 所依赖的通用请求/结果载体。 |

所有 `agent_string_view_t`、`agent_text_sink_t` 和 `agent_tool_call_view_t` 的成员均是借用值；
它们不隐含 NUL 终止、复制、释放或跨 callback 保留许可。具体借用期由拥有该 callback 的
领域接口说明。

### Model 专属类型

以下类型定义 Model Provider 的规范化输入投影，放在 `model.h`：

| 类型 | 原因 |
|------|------|
| `agent_model_t` | 仅由 Model 的构造、绑定、销毁与 Core 内部持有。 |
| `agent_model_workspace_t` | Model wrapper 的类型化 caller-storage，不包含 Provider 状态。 |
| `agent_message_role_t` | 是供应商无关的 Model transcript 角色，不是一般 Core 消息总线。 |
| `agent_message_view_t` | 仅在一次 Model completion 中借用的 transcript 项。 |
| `agent_tool_view_t` | 注册 Tool 面向 Model 的裁剪投影，依赖 `agent_tool_flags_t`。 |

`model.h` 显式 include `tool.h`，以使 `agent_tool_view_t.flags` 的位语义来自唯一的
`agent_tool_flags_t` 定义。Tool 注册描述符 `agent_tool_t`、回调和 policy 上下文继续归
`tool.h` 所有。

### 分层规则

```text
error.h
  -> types.h
       -> runtime.h / config.h / tool.h / context.h / session.h / event.h
       -> agent.h (同步运行与取消)
       -> model.h (Model transcript and Tool projection)
            -> transport.h and provider-specific extensions
```

这是 source-level include 方向，不承诺二进制 ABI。`types.h` 不得 include `model.h`、
`tool.h`、`context.h`、`session.h`、`event.h` 或平台头。领域头可 include
`types.h`，但不得通过间接 include 假定某个类型存在。

### 类型放置判定

新增 public struct 或 enum 时按以下顺序判断：

1. 它是否只描述一个领域的 callback、Provider 输入、注册项或状态机？是则放入该领域头。
2. 它是否在两个以上独立领域间交换，且没有一个领域天然拥有它？是则放入 `types.h`。
3. 它是否仅为私有实现或特定平台 Adapter 所需？放在 `src/` 内部头或平台扩展头，不进入
   `types.h`。
4. 它是否只是为避免 include 而创建的全局别名？不新增；直接 include 所属领域头。

`agent_error_t` 例外地位于 `error.h`，因为它是所有返回路径的最小依赖，`types.h` 只引用
它而不重新定义错误语义。

根据 ADR 0011，运行期容量规划不进入公共类型层；由当前 build Profile 决定大小与对齐的
`agent_workspace_t` 属于 `config.h` 的初始化 storage 契约，不属于跨模块值类型层。

## 不采用的方案

### 将所有公共类型留在 `types.h`

会使 Model transcript、Tool 投影、Transport、Plugin 等领域概念无边界地进入每个公共
编译单元；调用者难以判断所有权，且基础头会随扩展能力持续膨胀；拒绝。

### 将每类值拆成独立小头

将 string view、limits、request、response、stats、Tool call 分为多个头可以进一步细化，
但首版会扩大 include 图和文档导航成本。当前这些类型均是轻量 POD，保留一个基础数据层
更符合 MCU 与单文件应用的可用性；拒绝。

### 将 `agent_tool_call_view_t` 放入 `tool.h`

Tool call 同时是 Model 输出、Run confirmation 和 Event 载荷。让 Model、Run、Event 都
依赖完整 Tool 注册/执行 API 会扩大依赖面；保留在 `types.h`。

### 将 limits、request、response 与统计全部放入独立运行头

它们语义上与 turn 有关，但 Config、Context、Tool 和 Event 都直接使用其中的一部分。
单独为这些 POD 创建更多基础头没有实际运行时收益，且 `config.h` 与领域头的 include 图
更复杂；首版保留在 `types.h`。

## 影响

正面影响：

- Model 专属的 transcript 与 Tool projection 不再污染基础公共类型层；
- 所有无分配 view、workspace 预算、Tool call 与执行结果保持一处可查；
- 新模块可以按明确规则决定 API 类型归属，减少循环 include 与重复定义；
- 不改变 Core workspace、分配策略或平台 Port 的运行时成本。

代价：

- `model.h` 需要显式依赖 `tool.h`；这是 Model 输入依赖 Tool 投影的真实关系；
- `types.h` 仍包含 limits、request/response 和统计，不是严格意义上的“原子类型”头；
- 将来公开 Session transcript、异步 poll result 或 Plugin composition 时，可能需要新增
  领域头或修订本 ADR。

## 评审重点

1. `agent_cancel_token_t` 继续作为跨领域前置声明保留在 `types.h`，避免
   Model/Tool/Context 仅为不透明指针依赖 `agent.h`；`agent_turn_t` 不属于同步 MVP。
2. `agent_request_t`、`agent_response_t`、`agent_limits_t` 与统计是否继续集中在 `types.h`；
   当前判断是“保留”，避免首版过度拆头。
3. `agent_tool_call_view_t` 是否继续作为跨领域事实，而不下沉到 `tool.h`；当前判断是“保留”。
4. 未来若公开 Session 消息读取 API，是否应新建 `conversation.h`，而不是把更多 transcript
   类型重新放回 `types.h`；当前倾向是“新建领域头”。

## 验证要求

- 每个 public header 必须可单独在 C99 与 C++11 编译；
- 所有公共头的组合包含不得依赖偶然的 include 顺序；
- `types.h` 不得引入动态分配、平台 SDK、JSON 或网络依赖；
- Model、Tool、Run、Event 的声明必须继续使用同一份 `agent_tool_call_view_t`；
- Model Provider 输入中的 message/tool projection 类型只能由 `model.h` 定义。
