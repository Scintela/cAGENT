# Runtime Interface

Public header: `agent/runtime.h`. Runtime is an injected service table, not an
operating system or scheduler.

| Field/type | Required? | Contract |
|---|---|---|
| now_ms / clock_context | Clock required | Monotonic milliseconds, same Core/Provider reference |
| agent_allocator_t | Optional | Paired alloc/free; fixed storage needs neither |
| agent_sync_t cancel_sync | Optional | Paired enter/leave for short cancellation-state access |
| log / log_context | Optional | Callback-lived view, never leak credentials |

The table is copied; contexts remain application-owned until use ends. Runtime
is generally fixed at initialization; no public active replacement API exists.

Do not require threads, mutexes, events, files and networking in one large OSAL
table. Core runs synchronously in the application task; Transport/File Store
have their own contracts.

Inject synchronization for cross-task cancel; volatile alone is insufficient.
Hooks must not block on I/O. Ordinary APIs still need external serialization.
Builders differ: ESP-IDF/OpenVela minimally supply clocks; RT-Thread has explicit
state/timer lifetime. See [platforms](../platforms/index.md).
