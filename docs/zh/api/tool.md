# Tool 接口

状态：同步 Tool 模块已实现，代码提交 `965c93f`。接口定义在
`include/agent/tool.h` 和 `include/agent/policy.h`；架构见[Tool 模块](../arch/tool.md)。
**当前 `agent_run()` 的 ReAct 调度尚未实现**，注册工具不意味着已能通过 Core
完成模型调用、工具执行和 Session 提交。本文区分可调用的公共注册接口和已经完成、
等待 Run 接入的私有执行机制。

## 1. 类型与所有权

| 类型/字段 | 用途与寿命 |
|---|---|
| `agent_tool_t` | 工具贡献定义；注册表复制结构值，不深拷贝字符串或应用对象 |
| `name` | 非空、唯一；只允许 ASCII 字母、数字、`_`、`-`，长度不超过 `AGENT_MAX_NAME_BYTES` |
| `description` | 可空的 UTF-8 说明，受 `AGENT_MAX_DESCRIPTION_BYTES` 限制 |
| `input_schema_json` | 必需的完整 JSON 对象；字节和深度有界，不自动执行 JSON Schema 语义验证 |
| `group` / `category` | 可空的 UTF-8 分类文本，各受 `AGENT_MAX_NAME_BYTES` 限制；不表示权限或资源所有者 |
| `flags` | 已知标志的组合；未知位、同时声明 READ_ONLY/SIDE_EFFECT 均拒绝 |
| `validate` | 可选的纯语义验证回调；检查字段、类型、范围及未知字段策略，不操作设备 |
| `execute` | 必需的同步 handler；执行一次，通过 sink 输出结果文本 |
| `user_data` | 借用的设备/应用状态；由应用维护和释放 |
| `agent_tool_context_t` | validate/execute/Policy 的当次上下文；结构及成员不可在回调后保留 |
| `agent_text_sink_t` | handler 调用期间有效；write 返回前已复制文本，随后可复用源缓冲 |
| `agent_tool_view_t` | 私有注册表投影给 Model 的规范视图，不包含设备 handler |

注册后，名称、说明、schema、分类文本必须保持有效且不可修改，直到成功注销返回或
Agent 销毁。可以使用 Flash/只读存储里的字符串；不可借用会离开作用域的局部数组，
也不可借用 Core workspace/scratch 中的文本。`agent_tool_t` 定义结构本身可在注册成功后
离开作用域；应用状态则必须覆盖注册使用期。注销和 destroy 不释放应用资源。

所有操作遵循单 driver task 约定。回调中的重入保护不是线程锁；同实例上的注册、
查询和执行不能由其他任务并发调用。跨任务只使用符合 Runtime 同步约定的 `agent_cancel()`。

## 2. 公共函数

| 函数 | 行为 | 常见错误 |
|---|---|---|
| `agent_register_tool(agent, tool)` | CONFIGURING/READY 中完整校验后追加；失败不改变注册表 | INVALID、PARSE、LIMIT、EXISTS、CAPACITY、BUSY、NOT_SUPPORTED |
| `agent_unregister_tool(agent, name)` | 空闲时按名移除，释放借用关系；其余项保持相对顺序 | INVALID、NOT_FOUND、BUSY、NOT_SUPPORTED |
| `agent_tool_set_enabled(agent, name, enabled)` | 空闲时只修改 DISABLED；不改变 HIDDEN 或授权 | INVALID、NOT_FOUND、BUSY、NOT_SUPPORTED |
| `agent_tool_is_enabled(agent, name, out)` | driver 回调外查询是否没有 DISABLED；合法失败路径将 out 置 false | INVALID、NOT_FOUND、BUSY、NOT_SUPPORTED |
| `agent_tool_enumerate(agent, visitor, data)` | 注册顺序遍历全部工具，包括隐藏/禁用项；首个 visitor 错误停止并原样返回 | INVALID、BUSY、NOT_SUPPORTED，以及 visitor 错误 |
| `agent_set_policy_callback(agent, callback, data)` | 空闲时设置单个产品 Policy；NULL 清空并恢复默认拒绝 | INVALID、BUSY |

上述均返回 `agent_error_t`，仅 `AGENT_OK` 成功。查询的输出不得覆盖输入名称、借用元数据
或 Core workspace。枚举提供 callback-lifetime 的只读定义；需要跨回调使用时，由应用复制
必要内容。枚举期间不得注册/注销、改变 Policy、启动或销毁 Agent。

`AGENT_MAX_TOOLS == 0` 时不分配注册表，公共 Tool 注册/查询操作返回
`AGENT_ERROR_NOT_SUPPORTED`；启用 Tool 时，空注册表的枚举返回成功且不调用 visitor。
清空不存在的名称返回 NOT_FOUND，不自动当作成功。

## 3. 标志和授权

| 标志 | 行为 |
|---|---|
| `flags == 0` | 默认启用且可见；仍须 Policy 明确 ALLOW |
| `AGENT_TOOL_DISABLED` | 不进入 Model 投影，拒绝模型调用 |
| `AGENT_TOOL_HIDDEN` | 不进入投影，也拒绝模型按名调用；is_enabled 仍可能为 true |
| `AGENT_TOOL_READ_ONLY` | 应用声明无外部修改；不会自动授权 |
| `AGENT_TOOL_SIDE_EFFECT` | 应用声明可能修改设备/外部状态；库不自动重试或回滚 |
| `AGENT_TOOL_REQUIRES_CONFIRM` | 同步 MVP 拒绝执行，不建立暂停等待状态 |

没有 Policy、Policy 返回 DENY/CONFIRM/未知枚举值，都返回 POLICY_DENIED，不调用 handler。
REQUIRES_CONFIRM 即使 Policy 返回 ALLOW 也拒绝，且无需调用 Policy。
应用可在一个 Policy 回调中组合自己的规则；当前没有可注册的多 contributor Policy chain。

## 4. 回调上下文和参数

`agent_tool_context_t` 提供完整的 `call.id/name/arguments_json`、有效 session/trace、
limits、cancel token、绝对单调 deadline 和 request_user_data。Tool 的 `user_data`
是注册时的设备状态，`request_user_data` 是这次请求的应用状态，两者不能混淆。
call.id 必须非空；Session/trace 可以为空，三者均受 `AGENT_MAX_IDENTIFIER_BYTES` 限制。

Core 在执行前检查 arguments：完整对象、字节限制、UTF-8/转义、JSON 数字文法、深度，
以及所有嵌套对象的重复键。`"a"` 和 `"\u0061"` 被视作同一个键；NUL 键拒绝。
这不等于验证 required、enum、类型、数值范围或 additionalProperties，应用必须在
validate/execute 中落实所需语义。validate 在 Policy 前执行，必须无设备副作用。

JSON 字符串值中合法的 `\u0000` 不被全局禁止。应用解码时应使用长度，不允许因为
`strlen()` 或普通 C 字符串比较而忽略后缀；若业务不接受 NUL，由语义验证器拒绝。
库不公开 jsmn 类型，也不要求应用使用同一个 JSON 包来实现语义验证。

validate 返回 INVALID/PARSE/未分类 ERROR/正值，归一为 TOOL_ARGUMENT；其他负值保留。
handler 的未分类 ERROR/正值归一为 TOOL_FAILED，其他负值保留。不以错误码自动推断
设备动作是否发生。

## 5. 输出与时限

handler 调用 `sink->write(sink->context, text)` 输出长度感知的 UTF-8 文本，不必为 JSON。
不允许原始 NUL；可以按字节块分割 UTF-8，但返回时整体必须合法。成功可以不输出文本。
每次 write 均检查取消和 deadline，首个错误保持 sticky；后续 write 返回同一错误，
不再复制数据。handler 应立即传播错误；即使忽略错误，框架也不会返回整体成功。

Run 接入时须在 handler 前提供至少 `AGENT_MAX_TOOL_OUTPUT_BYTES + 1` 字节的目标缓冲，
否则返回 CAPACITY 且不执行。超过输出字节上限返回 LIMIT，不静默截断，不重新执行。
输出缓冲可以来自 Core scratch，不得与输入、上下文、limits、cancel token、注册文本或
持久 Core 状态重叠；应用还须保证不会覆盖其不透明 callback 状态。

有效 deadline 为调用者整体 deadline 与 `开始时刻 + per_tool_timeout_ms` 的更紧者；
0 表示未设置相应时限，加法饱和而非回绕。参数验证和 Policy 时间也计入工具时限。
取消/超时是协作式的，不能强制中断阻塞的设备函数，更不能撤销已发生的动作。

## 6. 装配示例

以下例子注册一个无参数的只读电源查询。backend 由应用实现，不需要平台 SDK 出现在
Tool API 中；它接受任意合法参数对象，不承诺拒绝未知字段。handler 和 Policy 不依赖
内置 Tool 包，也不会直接调用 Model。

```c
#include <agent.h>
#include <string.h>

typedef struct {
    agent_error_t (*read)(void* context, bool* on);
    void* context;
} app_power_source_t;

static agent_error_t read_power(void* data, const agent_tool_context_t* context,
                               const agent_text_sink_t* sink)
{
    app_power_source_t* source = data;
    bool on;
    agent_error_t status;
    if (agent_cancel_token_is_set(context->cancel)) return AGENT_ERROR_CANCELLED;
    status = source->read(source->context, &on);
    if (status != AGENT_OK) return status;
    return sink->write(sink->context, on ? agent_string_view("{\"on\":true}", 11u)
                                         : agent_string_view("{\"on\":false}", 12u));
}

static agent_policy_decision_t power_policy(void* data, const agent_policy_request_t* request)
{
    (void)data;
    if (request->tool->name.size == sizeof("get_power") - 1u &&
        memcmp(request->tool->name.data, "get_power", sizeof("get_power") - 1u) == 0)
        return AGENT_POLICY_ALLOW;
    return AGENT_POLICY_DENY;
}

agent_error_t app_register_power(agent_t* agent, app_power_source_t* source)
{
    agent_tool_t tool = {0};
    agent_error_t status;
    if (!source || !source->read) return AGENT_ERROR_INVALID;
    tool.name = agent_string_view("get_power", sizeof("get_power") - 1u);
    tool.description = agent_string_view("Read power state", sizeof("Read power state") - 1u);
    tool.input_schema_json = agent_string_view("{\"type\":\"object\"}", 17u);
    tool.flags = AGENT_TOOL_READ_ONLY;
    tool.execute = read_power;
    tool.user_data = source;
    status = agent_set_policy_callback(agent, power_policy, NULL);
    if (status != AGENT_OK) return status;
    return agent_register_tool(agent, &tool);
}
```

`source` 及其 backend context 必须覆盖注册期。此 helper 不提供两个 API 的事务回滚；
注册失败时 Policy 已安装，应用按装配流程处理。多工具产品应安装统一 Policy，而不是
每注册一项都替换它。示例仅装配，不通过私有接口伪造 `agent_run()` 已实现。

## 7. 容量配置

| 宏 | 作用 |
|---|---|
| `AGENT_MAX_TOOLS` | 常驻 registry 槽位；0 裁剪 Tool，无 implicit heap 扩容 |
| `AGENT_MAX_NAME_BYTES` / `AGENT_MAX_DESCRIPTION_BYTES` | 注册文本准入上限，不为每项预分配字符串数组 |
| `AGENT_MAX_SCHEMA_BYTES` | 单份注册 schema 的最大字节数 |
| `AGENT_MAX_ARGUMENTS_BYTES` | 单次调用参数对象的最大字节数 |
| `AGENT_MAX_JSON_DEPTH` | 对象/数组嵌套上限；Tool 启用时须为 1..32 |
| `AGENT_MAX_TOOL_OUTPUT_BYTES` | 单次 handler 的累计输出上限，不含 NUL |
| `AGENT_SCRATCH_BYTES` | Run 用于 view、调用参数副本、输出等的共享临时总预算 |
| `AGENT_CORE_WORKSPACE_BYTES` | Core 总预算；编译期检查 Agent + registry + 对齐 + scratch 是否装得下 |

`limits.max_tool_calls` 限制一轮尝试调用多少次 handler，不是可注册的工具数量。
被拒绝、未调用 handler 的请求不消耗该 handler 预算；模型迭代仍受 max_steps 限制。
整个 turn 的预算累计由后续 Run 编排维护，私有 Tool 入口只检查剩余额度。

普通 CMake 通过 `CONFIG_AGENT_*` 配置容量；直接编译可统一定义 `AGENT_*` 宏。
应用和库须使用同一 Profile。Tool 非零时 Core 自动依赖 reader；完整 reader/writer
及 OpenAI 仍通过 `AGENT_BUILD_JSON_CODEC=ON` 等选项启用。
