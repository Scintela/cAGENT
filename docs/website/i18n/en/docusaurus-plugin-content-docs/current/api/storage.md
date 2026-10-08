# Storage Providers and File Store

These are optional packages, not mandatory Core dependencies. Applications
supply state, buffers and mounted directories; build switches do not initialize
storage instances.

## RAM Session

Public header: `agent_session_ram.h`. Config supplies turns/messages/calls arrays,
payload, read_messages/read_calls and capacities. They must not overlap and must
outlive the binding. Initialize an empty volatile backend, export its binding
with `agent_session_ram_bind()`, then call `agent_set_session_storage()`.

Both metadata and payload limit capacity; a Turn count alone cannot predict
bytes. There is no file I/O, implicit heap or reboot persistence.

## JSONL Session

Headers: `agent_session_jsonl.h` and `agent_session_jsonl_files.h`.

| Configuration | Purpose |
|---|---|
| files / file_context | Session-specific file Ops/context |
| write_line | Encoded current Turn |
| read_line | One historical record |
| decoded | Decoded message/call strings |
| token_buffer / bytes | Private tokens with int alignment |
| read_messages / read_calls | Canonical complete-group arrays |
| session_id | Independent active transaction ID storage |

All buffers are caller-configured and nonoverlapping. Budget write_line for a
complete Turn with multiple iterations/results, not one message.

`agent_session_jsonl_files_init()` maps generic File Store to Session file Ops
using independent name scratch. Naming/batch semantics stay out of byte I/O.

```text
Platform File Store init
 -> session_jsonl_files_init (fills config.files/file_context)
 -> Complete JSONL working arrays and buffers
 -> session_jsonl_init -> session_jsonl_bind
 -> agent_set_session_storage
```

Init opens no Session file; binding transfers no backend ownership. See the
[Session guide](../guides/session.md) for records and failure boundaries.

## Shared Byte-File Interface

Header: `agent_file_store.h`. Binding is immutable Ops pointer + context.

| Interface | Behavior |
|---|---|
| agent_file_size | Query length |
| agent_file_read | Offset read; successful short reads allowed, zero is EOF |
| agent_file_read_exact | Require all bytes; unexpected EOF is IO |
| agent_file_read_all | Bounded whole-file read, not a concurrent-writer snapshot guarantee |
| agent_file_read_text | Whole text + NUL; rejects embedded NUL, consumer validates UTF-8 |
| agent_file_visit | Unordered ordinary filename enumeration, no sorting promise |
| agent_file_append / truncate / sync | Byte append/truncation/synchronization |
| agent_file_remove | Delete; may already be deleted on error |
| agent_file_replace | Whole-file replacement; published records publication facts |

Names are safe single components: no separators, NUL, dot directories or reserved
`.cagent-` prefix. Missing callbacks mean unsupported capabilities. Roots, state,
path scratch and I/O must satisfy nonoverlap rules.

An error with published=true does not mean the old file remains. sync is not
automatically power-loss safety; backend and filesystem determine guarantees.

## Markdown Memory

Header: `agent_markdown_memory.h`. Config maps soul/user/facts to Store, name and
body bound, plus an independent notes Store. Zero disables a document. Notes
use valid YYYY-MM-DD IDs; the backend does not scan all dates.

Init validates/copies mappings, bind exports Memory Ops, then agent_set_memory
attaches them. No cache, implicit persistent descriptor or heap is created.

## POSIX and Platform Stores

`agent_posix_file_store_init()` needs an existing absolute trusted directory,
independent path buffer, read_only and sync_directory. It does not mount, format
or create directories. ESP-IDF/OpenVela/RT-Thread wrappers reuse this backend.
Filesystems must support the selected capabilities; unsupported directory sync
cannot pretend success. See [platforms](../platforms/index.md).
