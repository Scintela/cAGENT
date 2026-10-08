# Event and Statistics APIs

Public header: `agent/event.h`.

| API | Contract |
|---|---|
| agent_set_event_callback | Idle replacement/removal of one observer; borrowed callback/user_data |
| agent_get_stats | Copy cumulative snapshot; serialize with other access |

Types are TURN_BEGIN, MODEL_BEGIN, MODEL_END, TOOL_BEGIN, TOOL_END and TURN_END.
There is no separate ERROR; end status reports failure. There are no
AGENT_START/END, incremental message or streaming token events.

`agent_event_t` contains type, monotonic timestamp_ms, Session/Trace IDs,
Tool-specific tool_call, status and current summary. References expire at
callback return; observers do not change execution results.

Callbacks run synchronously on the driver. No Agent reentry, including cancel.
Copy fields for asynchronous telemetry; events are not Session journals.
See [observability](../guides/observability.md).
