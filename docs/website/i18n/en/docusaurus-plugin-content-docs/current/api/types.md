# Shared Value Types

Public header: `agent/types.h`. It contains cross-module handles/value types,
not every domain structure.

## Text and Sinks

```c
typedef struct {
    const char* data;
    size_t size;
} agent_string_view_t;
```

`size` counts bytes, excluding an implicit terminator. Nonzero size needs a valid
pointer. `agent_string_view(data, size)` constructs a view without validation or
copying. Use `AGENT_SV_LITERAL("text")` for aggregate initialization and the
constructor for expressions. `agent_string_view_is_empty()` checks length only.

Do not pass an unterminated view to strlen, strcmp or printf `%s`. Views can
borrow RAM, constants or Provider buffers; each interface defines their lifetime.

`agent_text_sink_t` contains write and context. It consumes views synchronously;
producers check each result and neither save the sink nor write after return.

## Tool Calls

`agent_tool_call_view_t` carries call ID, Tool name and a complete
arguments_json object. It describes a model-produced call, not registered Tool
metadata. Model, Tool and Session share it; callback content is callback-lived.

## Limits

| Field | Unit and zero semantics |
|---|---|
| max_steps | Model iterations; must be nonzero |
| timeout_ms | Overall milliseconds; 0 disables this bound |
| per_model_timeout_ms | Per-model milliseconds; 0 leaves the overall deadline |
| per_tool_timeout_ms | Per-Tool milliseconds; 0 leaves the overall deadline |
| max_tool_calls | Handler attempts; 0 forbids Tools |
| max_output_tokens | Per-model token request budget; 0 unspecified |
| max_history_turns | Complete historical groups; 0 disables history projection |

`AGENT_LIMITS_DEFAULT` comes from build defaults. Limits are behavior budgets,
not allocation: twelve registered Tools and four handler attempts are separate.

## Requests, Results and Statistics

`agent_request_t` is one user request. `agent_response_t` separates execution,
Session and delivery results. `agent_run_summary_t` tracks model/Tool counts,
failures/denials, final result, elapsed time and peak scratch. `agent_stats_t` is
cumulative; counters saturate rather than wrap and contain no model usage.

See [Run](core.md) and [Event](event.md).
