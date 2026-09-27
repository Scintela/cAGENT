# ADR 0012: `agent.h` 的应用入口与生命周期边界

- 状态：提案
- 日期：2026-09-26

## 背景

cAgentV2 同时面向产品应用开发者、Model/Transport Provider 作者、平台 Port 作者和可选
addon 作者。若所有公共头都要求从 `agent.h` 间接包含，最小应用会被迫看到 Model、HTTP、
Transport、Context、Storage 和未来 Plugin 的扩展契约；若 `agent.h` 只包含 `agent_t`，普通
嵌入式应用又需要手工发现并包含过多基础头。

当前根头 `include/agent.h` 聚合配置、错误、基础类型、Tool、Policy、Event、Session 和版本，
并声明 Core 生命周期、同步运行和默认 limits 更新。`run.h`、`model.h`、`context.h`、`skill.h`
和 `transport.h` 独立存在。需要明确该布局不是偶然 include 顺序，也不意味着根头是所有
功能、所有平台和所有 Provider 的完整聚合入口。

根头承载 `agent_init()`、`agent_create()`、`agent_start()`、`agent_destroy()` 和 `agent_run()`。
这些函数需要形成一致的状态、workspace 和所有权语义，
避免应用误以为 `destroy` 可以中止活动 turn、`create` 是 MCU 的默认路径，或 `start` 会发起
模型网络请求。

## 决定

### `agent.h` 是常用应用入口，不是万能聚合头

`#include <agent.h>` 面向需要创建、配置、运行和观测一个典型 Agent 的产品应用。它聚合：

```text
error.h      types.h       config.h      version.h
tool.h       policy.h      event.h       session.h
```

这些头覆盖普通产品最常见的工作：准备配置、注册本地 Tool、设置 Policy/Event、管理内存
Session、运行同步请求和读取结果。聚合只影响编译期声明可见性；未使用模块不得因此成为链接
依赖、平台 SDK 依赖或运行时内存消耗。

`agent.h` 不声明或刻意聚合下列扩展契约：

| 需求 | 应显式包含的头 | 原因 |
|---|---|---|
| Model 创建、绑定、Provider ops | `agent/model.h` | Model 是外部、可替换 Provider。 |
| 分步 turn、确认与取消 token | `agent/run.h` | 仅高级驱动器需要显式状态机。 |
| 动态 Context Provider | `agent/context.h` | 可选 Context 贡献，不是最小运行必需项。 |
| Skill 注册 | `agent/skill.h` | 可选领域指导内容。 |
| Runtime 服务细节 | `agent/runtime.h` | `config.h` 为配置成员而依赖它；应用不得依赖该间接 include。 |
| HTTP Transport | `agent/transport.h` | Provider 扩展，不是所有 Model 都需要网络。 |
| Provider/Port 专用配置 | 对应扩展头 | 不进入通用 Core ABI。 |

一个使用 OpenAI-compatible Model 的应用应显式写出真实依赖：

```c
#include <agent.h>
#include <agent/model.h>
#include <agent/transport.h>
/* 以及 OpenAI Provider、目标平台 Transport 的扩展头。 */
```

根头不得 include 厂商 SDK、RTOS 头、HTTP/TLS/JSON 库类型或具体 Provider 头。每个公共头都
必须可独立以 C99 和 C++11 编译；任何头都不得依赖应用恰好先 include `agent.h`。

### 根头只拥有 Core 生命周期与同步便利入口

`agent.h` 自己声明的 API 仅包括：

```c
agent_error_t agent_init(agent_t** out, agent_workspace_t* workspace,
                         const agent_config_t* config);
agent_t* agent_create(const agent_config_t* config);
agent_error_t agent_start(agent_t* agent);
void agent_destroy(agent_t* agent);

agent_error_t agent_run(agent_t* agent, const agent_request_t* request,
                        agent_response_t* response);
agent_error_t agent_set_limits(agent_t* agent, const agent_limits_t* limits);
```

上述签名已经采用 ADR 0011 的编译期 workspace Profile。实现时必须保持 `agent.h`、
`config.h` 与 header tests 的同一容量模型，不能重新引入并行的 raw workspace
或运行期容量表路径。

领域 API 继续由所属头拥有：Model binding 在 `model.h`，turn step/resume/cancel 在 `run.h`，
Tool 注册在 `tool.h`，Context/Skill/Session/Policy/Event 操作在各自头中。不得仅因某个
类型已被根头聚合，就将新的领域函数添加到 `agent.h`。

`agent_run()` 是唯一保留在根头的运行便利函数，因为它是最小同步产品路径：内部等价于
begin/step/resume/end 的单一状态机封装，而非第二套 Loop 实现。需要用户确认、UI 逐步展示、
外部调度或细粒度取消的应用必须显式使用 `run.h`。

### 生命周期与 workspace 语义

根头 API 的有效状态转换为：

```text
agent_init
  -> CONFIGURING
  -> agent_start
  -> READY
  -> agent_run 或 agent_turn_begin
  -> ACTIVE
  -> 完成 / agent_turn_end
  -> READY
  -> agent_destroy
```

- `agent_workspace_t` 的大小与对齐由 build Profile 确定；它只包含 Core workspace，不包含
  Provider、Transport、TLS、JSON DOM 或任务栈。应用无需猜测 buffer 大小或调用预检 API。
  `agent/config.h` 公开 Profile 容量宏，`AGENT_CORE_WORKSPACE_BYTES` 是该 workspace 的
  总字节数，Core 在编译时检查其内部布局。
- `agent_init()` 绑定固定 workspace、初始化 pool 并完成配置验证；caller workspace 始终由
  调用方持有，Core 主路径不申请 heap。
- `agent_create()` 是使用 `config.runtime.allocator` 的可选 convenience path。它适合 Host 或
  明确允许 heap 的 Profile，不是 MCU 默认初始化方式。
- `agent_start()` 只验证 Core 已可进入 READY，不应发起 I/O 或模型请求。根据 ADR 0011，首版
  允许尚未绑定 Model 的 READY；真正需要模型的 run/turn 在使用点检查 binding。
- `agent_destroy()` 仅可在 CONFIGURING 或 READY 调用，且不得与 callback、Provider 或 active
  turn 并发。它是 NULL-safe 的最终释放操作，不是 cancel、stop、reset 或 rollback。对
  caller-provided workspace 不释放内存；对 create 路径和明确 owned binding 按所有权规则清理。

首版不在根头增加 `agent_deinit()`、`agent_stop()`、`agent_reset()`、`agent_restart()` 或
`agent_run_async()`。这些名字均引入不同的 Session、Provider、线程、已发生 Tool 副作用和
重配置语义；应在有真实产品行为与失败路径后由独立 ADR 设计。

### 默认 limits 是唯一的根级空闲态可变配置

`agent_set_limits()` 留在 `agent.h`，因为其修改的是所有后续 turn 都会继承的 Agent 默认
运行预算，属于 Core 生命周期使用者最常见的操作。它仅允许 CONFIGURING/READY，active turn
返回 `AGENT_ERROR_BUSY`；编译期资源容量、workspace、Runtime、system prompt 和已运行 turn
的 effective limits 均不受影响。

Model 更换、Tool/Context/Skill 注册、Policy/Event 替换和 Session 修改虽可在空闲状态发生，
但它们均拥有明确领域头，不能为“方便”迁入根头。

## 不采用的方案

### `agent.h` 聚合全部公共头

会使没有网络、没有 Model、没有 Context/Skill 或仅编写 Provider 的用户看到不相关 API；也会
让后续 Storage、Plugin 和平台扩展自然膨胀进最小应用入口；拒绝。

### `agent.h` 只声明 `agent_t` 与 init/destroy

最小化 include 图，但普通产品必须自行发现 config、request、response、Tool、Policy、Event
和 Session 的多个头；对嵌入式应用的首个可运行路径不友好；拒绝。

### 将所有 API 统一放在 `agent.h`

会消灭 Model、Run、Tool、Transport 等领域边界，增加同名和所有权语义冲突；拒绝。

### 让 `agent_create()` 成为默认入口

会将 heap 变成看似必要路径，削弱 caller-provided workspace 的确定性及 MCU 内存预算；拒绝。

### 在 `agent_destroy()` 中隐式取消、等待或终止 Provider

Core 无法通用地终止 HTTP、TLS、RTOS 线程或已发生的设备副作用。destroy 必须要求 idle；
取消、等待和 Provider 清理由应用及领域 API 显式完成；拒绝。

## 影响

正面影响：

- 普通嵌入式产品可从单一根头开始，同时 Provider/Port 仍按需依赖；
- 根头 API 数量稳定，生命周期语义集中且容易审查；
- 无 Model、Mock、本地 Model 和联网 Model 不会因 include/链接边界被混为一谈；
- caller workspace 路径保持主路径，heap 仅为明确的便利选择；
- 同步 `agent_run()` 与高级 turn API 共享一个实现，而不是形成两套执行逻辑。

代价：

- 使用 Model、turn step、Context 或 Transport 的应用必须多写显式 include；
- `agent.h` 聚合 Tool/Policy/Event/Session，仍不是严格的最小依赖头；
- 未来若同步应用普遍需要 Context/Skill，是否聚合它们需要重新评审，不能静默加入。

## 评审重点

1. `agent.h` 是否应继续聚合 Tool、Policy、Event、Session；当前结论是“保留”，它们是典型
   应用直接配置能力，且不引入厂商或平台依赖。
2. `agent_start()` 在无 Model 情形是否允许 READY；当前结论与 ADR 0011 一致，为“允许，run
   时检查”。需要同步修订 `public-api.md` 中的“必需能力”措辞。
3. `agent_workspace_t` 的公开形式是 union、struct 还是 Port 定义类型；当前要求是大小/对齐
   与当前 build Profile 一致，不能回退为调用方猜测的 raw byte array。
4. `agent_clear_model()` 是否首版公开；若接受，它属于 `model.h`，不加入根头。
5. `agent_create()` 是否在没有 allocator 的配置中返回 NULL 或通过其他路径报告错误；当前
   由实现和 API 文档在进入可链接阶段前明确。

## 验证要求

- `#include <agent.h>` 可独立在 C99 和 C++11 编译，并包含常用应用 API；
- `model.h`、`run.h`、`context.h`、`skill.h`、`transport.h` 均可不依赖 `agent.h` 独立包含；
- 最小本地/Mock Model 样例不链接 HTTP/TLS/JSON/平台网络依赖；
- caller workspace 的 init、各 build Profile 的 workspace 大小/对齐、可选 heap create 和
  destroy 所有权均有测试；
- `agent_run()` 与显式 turn 驱动在等价输入下遵循同一状态机、错误和 Tool 执行次数语义；
- ACTIVE 中 destroy、limits 更新和领域注册/替换均被拒绝，且不会破坏当前 turn。
