# Tool 与授权

Tool 是同步、带参数的应用能力，不等于 Skill。模型生成调用意图，Core 验证与授权，
应用 handler 操作设备或服务。注册 Tool 不表示默认授权。

## 定义一个只读工具

以下片段可加入已初始化的 Agent；完整启动代码见[快速开始](../getting-started/quickstart.md)。

```c
#include <agent.h>
#include <string.h>

static agent_error_t read_lamp(void* data,
    const agent_tool_context_t* context, const agent_text_sink_t* output)
{
    (void)data;
    if (agent_cancel_token_is_set(context->cancel))
        return AGENT_ERROR_CANCELLED;
    return output->write(output->context,
        agent_string_view("{\"on\":false}", 12u));
}

static agent_policy_decision_t allow_lamp(void* data,
    const agent_policy_request_t* request)
{
    (void)data;
    /* Match an explicit product capability, not a model-provided group label. */
    if (request->tool->name.size == 9u &&
        memcmp(request->tool->name.data, "read_lamp", 9u) == 0)
        return AGENT_POLICY_ALLOW;
    return AGENT_POLICY_DENY;
}

agent_error_t install_lamp(agent_t* agent)
{
    const agent_tool_t tool = {
        .name = AGENT_SV_LITERAL("read_lamp"),
        .description = AGENT_SV_LITERAL("Read the current lamp state"),
        .input_schema_json = AGENT_SV_LITERAL(
            "{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}"),
        .flags = AGENT_TOOL_READ_ONLY,
        .execute = read_lamp
    };
    agent_error_t status = agent_register_tool(agent, &tool);
    if (status != AGENT_OK) return status;
    return agent_set_policy_callback(agent, allow_lamp, NULL);
}
```

示例描述的是无参数只读操作。若必须拒绝所有非空参数对象，应增加 `validate`：
Core 校验 JSON 对象语法，不会代替应用执行 `additionalProperties` 等 Schema 语义。

## 安全执行链

```text
模型完整 Tool Call
  → 查名称与启用/可见状态
  → 调用预算、取消与 deadline
  → arguments JSON 语法
  → 应用 validate：类型、范围、必需字段
  → Policy：用户、设备和操作权限
  → execute：设备操作
  → 有界结果与执行事实
  → Session 配对结果
  → 下一次模型调用
```

Schema 是模型输入和语法准入的一部分，不是权限证明。
`READ_ONLY` 是应用声明，不是沙箱。安全检查应在访问设备之前完成。

## 输出与失败

输出通过 `agent_text_sink_t` 写入，检查每次返回值；Core 收集并验证完整 UTF-8。
结果可为文本或 JSON 文本，由产品约定；不允许超限后默默丢弃尾部。

handler 返回失败不意味着动作未执行。超时、取消、网络异常后设备状态可能未知；
设计幂等命令、动作 ID 和状态查询，禁止无条件重试副作用工具。

当前同步 API 没有确认恢复机制。`REQUIRES_CONFIRM` 或 Policy 的 CONFIRM 会阻止执行，
不会自动批准。需要用户确认的产品应先在应用层完成确认，再提交明确授权的请求。

函数、字段和标志详见[Tool 参考](../api/tool.md)，实现链见[Tool 架构](../arch/tool.md)。
