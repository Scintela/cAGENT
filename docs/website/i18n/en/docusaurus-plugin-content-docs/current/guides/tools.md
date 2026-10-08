# Tools and Authorization

A Tool is a synchronous parameterized application capability, not a Skill. The
model proposes a call, Core validates and authorizes it, and the application
handler operates the device/service. Registration does not grant permission.

## Define a Read-Only Tool

Add this to an initialized Agent; see the [quickstart](../getting-started/quickstart.md)
for a complete startup example.

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

This is a parameterless read-only operation. To reject every nonempty argument
object, add `validate`: Core checks JSON object syntax but does not implement
`additionalProperties` or other JSON Schema semantics for the application.

## Guarded Execution

```text
Complete model Tool Call
  -> Name lookup and enabled/visible state
  -> Call budget, cancellation and deadline
  -> arguments JSON syntax
  -> Application validate: types, ranges, required fields
  -> Policy: identity, device and operation permissions
  -> execute: device operation
  -> Bounded output and execution facts
  -> Paired Session result
  -> Next model call
```

Schema is model input and admission metadata, not proof of authorization.
`READ_ONLY` is an application assertion, not a sandbox. Check safety before
accessing the device.

## Output and Failures

Write through `agent_text_sink_t` and check every return value. Core collects
and validates complete UTF-8 output. Results may be plain text or JSON text by
product convention; exceeding bounds must not silently discard the tail.

A handler error does not prove that no action happened. Device state may be
unknown after timeout, cancellation or network failure. Use idempotent commands,
operation IDs and state queries; never unconditionally retry side effects.

The synchronous API has no confirmation resume. `REQUIRES_CONFIRM` and Policy
CONFIRM block execution rather than approving it. Complete user confirmation
in the application before submitting an explicitly authorized request.

See the [Tool reference](../api/tool.md) and [Tool architecture](../arch/tool.md).
