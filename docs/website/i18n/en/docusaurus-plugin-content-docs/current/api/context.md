# Context Interface

Unified bounded projection combines Session, Tool, Skill and Memory in
`agent_run()`. Applications register sources; Run prepares snapshots and
per-model projections. There is no public manual build API.

## Dynamic Contributions

Public header: `agent/context.h`. Synchronous callbacks run on the driver and
refresh for each model call, without background threads.

```c
#include <agent.h>
#include <agent/context.h>

static agent_error_t build_state(void* data, const agent_context_request_t* request,
                                  const agent_text_sink_t* output)
{
    (void)data;
    if (agent_cancel_token_is_set(request->cancel)) return AGENT_ERROR_CANCELLED;
    return output->write(output->context, agent_string_view("lamp=off", 8u));
}

agent_error_t register_state(agent_t* agent)
{
    const agent_context_provider_t source = {
        .name = AGENT_SV_LITERAL("device-state"),
        .priority = 10,
        .required = true,
        .build = build_state,
        .user_data = NULL,
        .max_bytes = 128u,
        .placement = AGENT_CONTEXT_REFERENCE
    };
    return agent_register_context(agent, &source);
}
```

The example is nonblocking. Real device/network reads must poll cancellation and
deadlines. Core checks at boundaries and sink writes but cannot preempt blocked
callbacks. `agent.h` declares `agent_cancel_token_is_set()`.

| Field | Contract |
|---|---|
| name | Unique ASCII identifier, same rules as Skills |
| priority | Presentation order within a source class; stable ties |
| required | Failure stops building; declared upper bound is reserved |
| build | Required synchronous callback; sink copies text, never retain pointers |
| user_data | Application-owned through registration lifetime |
| max_bytes | Body upper bound; 0 uses profile bound; REFERENCE also obeys input-message cap |
| placement | INSTRUCTIONS adds system text; REFERENCE creates temporary USER data |

Set a realistic max_bytes explicitly. Admission uses declared bounds, not an
optimistic expected output size. With a 4096-byte profile text cap, one required
4096-byte contribution cannot coexist with nonempty fixed instructions.

Collect required before optional contributions, sorted by priority within each
group. Presentation restores registry priority order. Callbacks should be
read-only and not depend on other callbacks' side effects. Chunks may split
UTF-8; validate the complete contribution. The first sink error remains sticky.

## Select Memory

Bind a Provider, then select logical documents. Registration performs no I/O:

```c
agent_error_t register_facts(agent_t* agent)
{
    const agent_memory_context_t facts = {
        .name = AGENT_SV_LITERAL("preferences"),
        .key = { .kind = AGENT_MEMORY_FACTS },
        .priority = 0,
        .required = false,
        .max_bytes = 512u
    };
    return agent_register_memory_context(agent, &facts);
}
```

SOUL becomes trusted instructions. USER/FACTS/NOTE become temporary USER-role
references. NOTE needs nonempty key.id; Provider maps dates/files. Selection does
not scan all notes or authorize model writes/Tools. Role/content partitions are
not a prompt-injection guarantee; the application owns SOUL trust.

Dynamic sources and Memory selections share AGENT_MAX_CONTEXTS and a namespace.
`agent_unregister_context()` removes either kind. Descriptors are copied;
names, NOTE IDs and state remain borrowed. Registration may precede Memory
binding; unbound projection returns NOT_SUPPORTED according to requiredness.

## Errors, Budgets and Lifetimes

Register/unregister only in CONFIGURING/READY. ACTIVE/reentry is BUSY, invalid
arguments INVALID, UTF-8 PARSE, excessive declarations LIMIT, duplicates EXISTS,
full slots CAPACITY and zero slots NOT_SUPPORTED.

- Required-source failure propagates and publishes no model input.
- Optional-source failure discards the whole block and records name/status privately.
- CANCELLED/TIMEOUT always terminate, even for optional sources.
- Text exhaustion is CONTEXT_OVERFLOW; scratch exhaustion CAPACITY; descriptors LIMIT.
- Text includes fixed instructions, Skills, Memory and dynamic bodies, conservatively reserving two separator bytes per nonempty contribution except the first.
- History obeys separate message, descriptor and remaining-scratch bounds; these are not token estimates.

Memory snapshots remain stable per Turn; dynamic data refreshes per model call.
Public Memory commands are idle-only, not callable by callback reentry. Private
projection reads selected documents; references do not become persistent
Session records. See [Context architecture](../arch/context.md).
