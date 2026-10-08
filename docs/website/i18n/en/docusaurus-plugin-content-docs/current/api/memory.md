# Memory Interface

Public header: `agent/memory.h`. Memory is a long-term content contract, not a
Session cache or vector retrieval API.

| Type | Meaning |
|---|---|
| agent_memory_kind_t | SOUL, USER, FACTS, NOTE |
| agent_memory_key_t | kind + id; only NOTE requires a logical ID |
| agent_memory_change_t | UNCHANGED, APPLIED, UNKNOWN change facts |
| agent_memory_ops_t | Required read; optional replace/forget |
| agent_memory_t | Copied Ops + borrowed context |

## Functions

| Function | Behavior |
|---|---|
| agent_set_memory | Idle binding/replacement; NULL unbinds; no implicit Provider destruction |
| agent_memory_read | Idle whole-document copy to caller buffer, returning a NUL-terminated view |
| agent_memory_replace | Trusted application whole-document replacement; rejects SOUL |
| agent_memory_forget | Trusted application deletion; rejects SOUL; not secure erasure |

Read capacity includes terminator space; max_bytes bounds body size. No silent
truncation. The view borrows the caller's buffer, not backend cache.

Check status and change together: failed replacement/deletion may be APPLIED or
UNKNOWN, so do not retry from status alone. APIs infer no model authority and
permit no reentry from active Tool callbacks.

Binding and projection are separate: use `agent_register_memory_context()` to
select documents. See [Memory](../guides/memory.md) and [Storage](storage.md).
