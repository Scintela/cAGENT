# Tool Interface

Public headers: `agent/tool.h` and `agent/policy.h`. Synchronous registration,
authorization and execution are integrated with `agent_run()`, Session and
events. Private invocation APIs are not application entry points. See the
[guide](../guides/tools.md) and [architecture](../arch/tool.md).

## 1. Types and Ownership

| Type/field | Contract |
|---|---|
| agent_tool_t | Copied definition; strings/application objects are not deep-copied |
| name | Nonempty unique ASCII letters/digits/underscore/hyphen; bounded by AGENT_MAX_NAME_BYTES |
| description | Optional UTF-8; bounded by AGENT_MAX_DESCRIPTION_BYTES |
| input_schema_json | Required complete bounded JSON object; no automatic JSON Schema semantic validation |
| group / category | Optional UTF-8, each bounded by AGENT_MAX_NAME_BYTES; not authority or ownership |
| flags | Known bits only; READ_ONLY and SIDE_EFFECT cannot coexist |
| validate | Optional pure business validation, no device actions |
| execute | Required synchronous handler, emitting result through sink |
| user_data | Borrowed application/device state |
| agent_tool_context_t | Current validate/execute/Policy context; expires at callback return |
| agent_text_sink_t | Valid during handler; write copies before returning, permitting source-buffer reuse |
| agent_tool_view_t | Canonical Model projection without device handlers |

Names, descriptions, Schema and classification strings remain immutable and
alive until successful unregistration or Agent destruction. Flash/constants are
valid; temporary arrays and Core Workspace/scratch text are not. The definition
struct may leave scope after registration, but application state must remain.
Neither unregistration nor destruction frees application resources.

Use one driver task. Callback guards are not locks; do not concurrently register,
query or execute on the same instance. Cross-task cancellation alone follows
the separate Runtime synchronization contract.

## 2. Public Functions

| Function | Behavior | Common errors |
|---|---|---|
| agent_register_tool | Validate then append in CONFIGURING/READY; failure leaves registry unchanged | INVALID, PARSE, LIMIT, EXISTS, CAPACITY, BUSY, NOT_SUPPORTED |
| agent_unregister_tool | Remove by name while idle; preserve other entries' order | INVALID, NOT_FOUND, BUSY, NOT_SUPPORTED |
| agent_tool_set_enabled | Change DISABLED while idle, not HIDDEN or authorization | INVALID, NOT_FOUND, BUSY, NOT_SUPPORTED |
| agent_tool_is_enabled | Query absence of DISABLED outside callbacks; valid failure paths set out false | INVALID, NOT_FOUND, BUSY, NOT_SUPPORTED |
| agent_tool_enumerate | Registration-order traversal, including hidden/disabled entries; propagate first visitor error | INVALID, BUSY, NOT_SUPPORTED, visitor status |
| agent_set_policy_callback | Set one idle product Policy; NULL restores default deny | INVALID, BUSY |

Only AGENT_OK succeeds. Query output must not overwrite names, borrowed metadata
or Core Workspace. Enumeration views are callback-lived; copy for later use.
Do not mutate, start or destroy Agent during enumeration.

With `AGENT_MAX_TOOLS=0`, no registry is allocated and Tool registration/query
returns NOT_SUPPORTED. An enabled empty registry enumerates successfully with
no callbacks. Removing an absent name returns NOT_FOUND, not success.

## 3. Flags and Authorization

| Flag | Effect |
|---|---|
| flags == 0 | Enabled and visible, still requires explicit Policy ALLOW |
| AGENT_TOOL_DISABLED | Omitted from projection and rejected on invocation |
| AGENT_TOOL_HIDDEN | Omitted/rejected; is_enabled may still return true |
| AGENT_TOOL_READ_ONLY | Application assertion of no external mutation, not automatic permission |
| AGENT_TOOL_SIDE_EFFECT | May mutate external state; no automatic retry/rollback |
| AGENT_TOOL_REQUIRES_CONFIRM | Rejected by synchronous MVP, no paused state |

Absent Policy or DENY/CONFIRM/unknown decisions return POLICY_DENIED without
calling the handler. REQUIRES_CONFIRM rejects even ALLOW and need not call
Policy. Compose rules inside one application callback; there is no contributor
Policy-chain registration.

## 4. Context and Arguments

Context includes complete call.id/name/arguments_json, effective Session/Trace,
limits, cancel token, absolute monotonic deadline and request_user_data. Tool
user_data is registered device state; request_user_data is this request's state.
Call IDs must be nonempty. Session/Trace may be empty; identifiers use
AGENT_MAX_IDENTIFIER_BYTES.

Core checks complete-object syntax, bytes, UTF-8/escapes, number grammar, depth
and duplicate keys in all nested objects. `"a"` and `"\u0061"` are equivalent;
NUL keys are rejected. Required fields, enum, types, ranges and
additionalProperties remain application responsibilities. validate runs before
Policy and must have no device side effects.

Legal `\u0000` string values are not globally forbidden. Decode with lengths so
C string operations cannot hide suffixes; reject NUL in business validation if
needed. The library exposes no jsmn types and does not mandate the application's
JSON package.

INVALID/PARSE/unclassified ERROR/positive validate results normalize to
TOOL_ARGUMENT; other negatives remain. Unclassified ERROR/positive handler
results normalize to TOOL_FAILED; other negatives remain. Codes do not prove
whether device actions occurred.

## 5. Output and Deadlines

Write length-aware UTF-8 through the sink; JSON is optional. Raw NUL is forbidden.
Chunks may split UTF-8 characters, but final collected text must be valid.
Successful handlers may emit nothing. Every write checks cancellation/deadline;
the first error sticks, later writes return it without copying. Propagate errors
immediately; ignoring them cannot make the overall call succeed.

Run reserves at least `AGENT_MAX_TOOL_OUTPUT_BYTES + 1` bytes before execution;
otherwise CAPACITY prevents the handler. Exceeding the output cap returns LIMIT,
without truncation/reexecution. Scratch output must not overlap input, context,
limits, cancel, registered text or persistent Core state. The application also
protects opaque callback state from overlap.

The effective deadline is the tighter of overall deadline and start plus
per_tool_timeout_ms. Zero disables that bound; addition saturates. Validation
and Policy count against the Tool budget. Cooperative timeout/cancel cannot
preempt blocking functions or undo actions.

## 6. Assembly Example

The application supplies a read-only power backend, with no SDK types in the
Tool contract. This example accepts any syntactically valid object and does not
reject unknown fields. It neither depends on built-in Tools nor calls Model.

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

Keep source/backend context alive while registered. This helper does not roll
back both APIs transactionally: Policy remains installed if registration fails.
Multi-Tool products should install one combined Policy rather than replace it
for every registration. This is assembly code, not private invocation bypass.

## 7. Capacity Configuration

| Macro | Purpose |
|---|---|
| AGENT_MAX_TOOLS | Persistent slots; zero trims, no implicit heap growth |
| AGENT_MAX_NAME_BYTES / AGENT_MAX_DESCRIPTION_BYTES | Admission bounds, not per-entry string arrays |
| AGENT_MAX_SCHEMA_BYTES | One registered Schema |
| AGENT_MAX_ARGUMENTS_BYTES | One argument object |
| AGENT_MAX_JSON_DEPTH | Nesting depth, 1..32 when enabled |
| AGENT_MAX_TOOL_OUTPUT_BYTES | Cumulative handler body excluding NUL |
| AGENT_SCRATCH_BYTES | Shared temporary views, copied arguments and output |
| AGENT_CORE_WORKSPACE_BYTES | Total; assertions check state, registries, alignment and scratch |

limits.max_tool_calls bounds handler attempts per Turn, not registry count.
Rejected calls without a handler consume no handler budget; max_steps still
limits model iterations. Run tracks cumulative attempts; private Tool invocation
checks only the remaining allowance.

Ordinary CMake accepts CONFIG_AGENT_*; direct builds may define AGENT_*
consistently project-wide. Nonzero Tool capacity requires the reader. Full
reader/writer and OpenAI remain optional build selections.
