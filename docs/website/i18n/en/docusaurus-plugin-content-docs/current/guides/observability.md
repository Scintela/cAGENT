# Events, Statistics and Cancellation

## Register an Observer

```c
#include <agent.h>
#include <stdio.h>

static void on_event(void* data, const agent_event_t* event)
{
    (void)data;
    printf("event=%d status=%s models=%u tools=%u\n",
        (int)event->type, agent_error_str(event->status),
        (unsigned int)event->summary.model_calls,
        (unsigned int)event->summary.tool_calls);
}
```

Call `agent_set_event_callback(agent, on_event, NULL)` while idle. Avoid blocking
printing in production callbacks; copy bounded fields and enqueue asynchronous
reporting in the application.

## Turn Boundaries

```text
TURN_BEGIN
  MODEL_BEGIN -> MODEL_END
  TOOL_BEGIN  -> TOOL_END       Zero or more Tools
  MODEL_BEGIN -> MODEL_END      Further iterations if needed
TURN_END
```

MODEL_END and TOOL_END carry failure statuses; TURN_END is terminal. A generic
ERROR event is not needed to detect failure. Tool boundaries describe actual
handler invocation: rejection before execution does not imply handler events.
Events are not a recoverable persistence journal.

Admission failures happen before a Turn begins and need not emit TURN_BEGIN/END.
Once begun, normal completion, cancellation and failure use the finalization path.

## Statistics and Delivery

`agent_get_stats()` returns a cumulative snapshot; serialize access.
`response.summary` contains Turn facts, while `response.stats` contains the
post-run cumulative snapshot. Statistics track calls, elapsed time and peak
scratch, not model token usage.

`agent_run()` returns execution status. `delivery_status` separately describes
final-text truncation; a small output buffer does not automatically rerun Tools.

## Cooperative Cancellation

The worker executes Run. Another task may call `agent_cancel()` with Runtime
synchronization configured. Core, Provider and long-running Tools poll borrowed
tokens and deadlines. A blocked SDK may delay cancellation; do not free buffers
still in use. Do not cancel by reentering a callback; route that intent through
external application control. See [Event](../api/event.md).
