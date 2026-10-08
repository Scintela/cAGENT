# API Overview

This section documents callable APIs, structures and current contracts, not V1
migration proposals or future declarations. Interfaces use the `agent_` prefix;
status-returning operations use `agent_error_t`, with only AGENT_OK successful.

Current source headers are authoritative. Build application, Core and Providers
from one revision and capacity configuration. Binary ABI compatibility across
versions, platforms or profiles is not promised.

## Find an API by Task

| Task | Public header | Reference |
|---|---|---|
| Initialize, start, run, cancel | `agent.h` | [Lifecycle and Run](core.md) |
| Capacities, Workspace, defaults | `agent/config.h` | [Configuration](config.md) |
| Handles, text, limits, requests/results | `agent/types.h` | [Value types](types.md) |
| Error classification and names | `agent/error.h` | [Errors](error.md) |
| Tool registration and callbacks | `agent/tool.h` | [Tool](tool.md) |
| Tool authorization | `agent/policy.h` | [Policy](policy.md) |
| Static instructions | `agent/skill.h` | [Skill](skill.md) |
| Dynamic data and Memory selection | `agent/context.h` | [Context](context.md) |
| Session Storage and queries | `agent/session.h` | [Session](session.md) |
| Whole long-term documents | `agent/memory.h` | [Memory](memory.md) |
| Events and cumulative statistics | `agent/event.h` | [Event](event.md) |
| Model implementations and binding | `agent/model.h` | [Model](model.md) |
| Clock, allocation, logs, cancel synchronization | `agent/runtime.h` | [Runtime](runtime.md) |
| Synchronous HTTP backends | `agent/transport.h` | [Transport](transport.md) |
| Optional RAM, JSONL, File Store, Markdown | Independent Provider headers | [Storage](storage.md) |

`agent.h` aggregates common entry points, not SDKs or every Model, Memory, Skill
or Context API. Include those headers explicitly. There is no public `run.h`,
`plugin.h`, `agent_plan()` or Turn step/resume API.

## Common Rules

- Views are `data + size`, not necessarily NUL-terminated strings.
- Registration/binding is often shallow; copying a descriptor does not copy text or context.
- Serialize ordinary operations on the driver task; do not mutate ACTIVE instances or reenter callbacks.
- Callback views expire when the callback returns; do not retain sinks or call them asynchronously.
- Budget Provider, HTTP/TLS, Storage and task stacks outside Core.
- Execution failure does not prove no external effect; persistence failure does not prove no publication.
- See the [module map](../arch/module-map.md) and [platform scope](../platforms/index.md).

New users should begin with the [quickstart](../getting-started/quickstart.md),
not assemble an application by guessing Ops contracts.
