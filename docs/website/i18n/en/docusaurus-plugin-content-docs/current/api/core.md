# Lifecycle and Run APIs

Public header: `agent.h`. Instance operations require valid live handles. These
APIs do not create application threads, queues or network connections.

## Lifecycle

| API | Purpose | Ownership/failure |
|---|---|---|
| `agent_init(&agent, &workspace, &config)` | Initialize caller storage, enter CONFIGURING | NULL output on failure; does not free Workspace |
| `agent_create(&config)` | Allocate Workspace and initialize | NULL on failure; paired alloc/free required |
| `agent_start(agent)` | Enter READY after state checks | No I/O; Model may be absent |
| `agent_destroy(agent)` | Clean idle instance and owned Model | NULL-safe; ignored during ACTIVE/callbacks, not deferred destruction |

Workspace is persistent instance storage, not a temporary argument. Do not
reinitialize storage while its original instance is alive.

## Synchronous Run

```c
agent_error_t agent_run(agent_t* agent,
    const agent_request_t* request, agent_response_t* response);
```

The instance must be READY. Supply valid nonempty input. An empty Session ID
selects the default Session. Initialize `response.output` and
`response.output_size` before calling.

| Field | Meaning |
|---|---|
| request.session_id / trace_id | Borrowed logical IDs, not paths or credentials |
| request.input | Immutable UTF-8, alive until return |
| request.limits | NULL inherits; non-NULL completely overrides and is copied at entry |
| request.user_data | Product context forwarded to Context, Tool and Policy |
| response.status | Terminal execution status corresponding to the return value |
| response.session_status | First Session transaction error, not a durability guarantee |
| response.delivery_status | Final-text delivery status |
| output_written / required | Delivered/full text bytes, excluding terminator |
| output_truncated | Insufficient output, delivered on UTF-8 boundaries |
| summary / stats | Turn facts and cumulative snapshot |

Zero capacity permits no text delivery; nonzero capacity needs a valid writable
pointer. Successful execution may still have TRUNCATED delivery. Do not rerun
for truncation alone. Storage failure, model failure, side effects and delivery
are separate facts.

After admission the instance enters ACTIVE. Finalization handles Session,
events, statistics and scratch, then restores READY. Failure/cancellation leaves
no public resumable Turn.

## Cancellation and Limits

| API | Contract |
|---|---|
| `agent_cancel(agent)` | Request cooperative cancellation; idle no-op; not ISR-safe |
| `agent_cancel_token_is_set(token)` | Poll the borrowed current token in long callbacks |
| `agent_set_limits(agent, &limits)` | Change defaults while idle, without changing capacities |

Inject synchronization for cross-task cancellation. Do not reenter Agent from
callbacks or treat cancel as a replacement for waiting for Run to finish.
See the [Run state machine](../arch/run.md).
