# Lifecycle and Concurrency

## Assemble an Instance

```text
Supply Runtime and Workspace
  -> agent_init()                    CONFIGURING
  -> Bind Model / Storage / Memory
  -> Register Tool / Skill / Context
  -> Set Policy and Event callback
  -> agent_start()                   READY
  -> agent_run()                     ACTIVE
  -> Return                          READY
  -> Reconfigure or destroy while idle
```

`agent_start()` neither creates a thread nor connects an LLM, and does not start
a Session. The request supplies its Session ID. One `agent_run()` is one user
Turn and can contain several model calls.

Starting without a Model is allowed for configuration/testing; runs that need a
Model fail at the point of use. Only one Turn can be driven per instance. Do not
replace Models, register Tools or write Memory during execution.

## Resource Ownership

| Resource | Owner | Minimum lifetime |
|---|---|---|
| Core Workspace passed to `agent_init()` | Application | Until Agent destruction |
| Core Workspace from `agent_create()` | Agent's paired allocator | Freed at destruction |
| Default system prompt, Runtime context | Application | While used by Agent |
| Tool/Skill strings and user_data | Application; registration is shallow | Until unregistration/destruction |
| Wrapper/Provider bound with `agent_set_model()` | Application | Until replaced or Agent destroyed |
| Wrapper bound with `agent_set_model_owned()` | Transferred only on success | Cleaned at replacement/destruction |
| Storage/Memory context and buffers | Application; Ops copied per contract | Until all use ends and binding is removed |
| Model/Tool/Event callback views | Producer | Until that callback returns |
| Request input and user_data | Caller | Until `agent_run()` returns |
| Final output buffer | Caller | Remains caller-owned after return |

Destroying a wrapper does not free arbitrary Provider memory. Optional
`ops.destroy` performs Provider-specific cleanup; the wrapper manages only its
own storage. Fixed storage does not require an allocator.

## Multitasking Applications

Let UI/voice tasks enqueue requests, and have one Agent worker call `agent_run()`.
Core creates neither tasks nor queues. Serialize queries, registration,
destruction and shared Store operations in the application.

Cross-task `agent_cancel()` requires paired Runtime `cancel_sync.enter/leave`
hooks. This short critical section does not make the whole Agent thread-safe.
Cancellation is not ISR-safe. Wait for the worker and all consumers to finish
before destroying Runtime, Provider or Workspace.

## Callback Rules

- Callbacks run synchronously on the driver; do not reenter Agent, including cancel from Event callbacks.
- Events are observational. Copy needed fields before enqueueing asynchronous work.
- Do not retain sinks, tokens or callback views; copy data for delayed use.
- Core cannot preempt a blocked SDK; long operations must poll cancellation and absolute deadlines.
- Workspace, input, output and Provider buffers must not overlap illegally.

See [Run APIs](../api/core.md), [Runtime](../api/runtime.md) and
[security boundaries](security.md).
