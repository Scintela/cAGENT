# Session Interface

Public header: `agent/session.h`. Domain contracts contain no JSONL, paths or SDKs.

## Storage Ops

| Operation | Contract |
|---|---|
| begin | Start this Turn's transaction by Session ID |
| append | Append canonical messages; borrowed during call, never retain dangling views |
| finish | End COMPLETE/ABORTED; consume the transaction even on failure |
| recent | At most max_candidates complete model-safe groups, newest first |
| clear | Clear one conversation; backend defines identity retention |
| clear_all | Batch clear; failure can leave partial changes |
| remove | Delete a conversation and identity; error does not prove it remains |
| count | Number of Session identities known to backend |

`agent_session_group_view_t` contains messages/count and lives only during the
recent visitor. A group missing Tool results must not be marked complete. Core
copies selected history into bounded scratch.

`agent_session_storage_t` contains Ops values and context. Idle
`agent_set_session_storage(agent, &storage)` copies the binding; NULL disables
Storage. Application-owned context/buffers are not freed with Agent. All eight
callbacks must be non-NULL; unsupported operations return NOT_SUPPORTED. This
differs from optional File Store callbacks.

## Application Queries

`agent_session_clear()`, `clear_all()`, `remove()` and `count()` forward to
Storage from idle driver context. Unsupported operations return NOT_SUPPORTED;
delete/clear failures provide no rollback guarantee.

See the [Session guide](../guides/session.md) and [Storage reference](storage.md).
