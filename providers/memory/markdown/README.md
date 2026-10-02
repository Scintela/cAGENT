# Markdown Memory Provider

Optional whole-document Memory over the shared File Store. Core uses only the
public `agent/memory.h` domain contract; this package maps authorized documents
and dates to file names without depending on a filesystem SDK or JSON codec.

Configure `soul`, `user`, `facts` with a borrowed store, a flat file name and a
positive byte limit. Example names are `SOUL.md`, `USER.md`, `MEMORY.md`; they are
application configuration, not Core constants. A zero limit disables a document.
At least one document or notes namespace must be enabled. Initialization does
not access files, mount a filesystem, create directories or seed content.

`notes` uses a separate application-authorized store/root. `AGENT_MEMORY_NOTE`
keys carry a trusted `YYYY-MM-DD` date and map to `YYYY-MM-DD.md`. Calendar checks
do not establish date provenance. Applications must guarantee actual root and
user isolation, including distinct store contexts that might alias one directory.

Initialize `agent_markdown_memory_t`, export `agent_memory_t` with
`agent_markdown_memory_bind`, then call `agent_set_memory`. Core copies the
binding but does not own the provider. Keep provider, immutable file ops/context,
names and platform path buffers alive; unbind and finish all calls before
reinitializing/freeing them. Access is synchronous, caller-serialized and idle-only
through the application API. No heap, content cache or retained file descriptor
is introduced. Do not overlap caller buffers with provider/store state or names.

Reads return complete text in caller-owned storage with room for a NUL terminator.
No silent truncation or embedded NUL; missing files are NOT_FOUND, disabled or
missing capabilities are NOT_SUPPORTED. No Markdown or UTF-8 interpretation is
claimed. Exclude all concurrent file edits during reads; the byte helper is not a
filesystem snapshot. A completed memory copy remains valid until its buffer is
reused, independently of subsequent publications.

Ordinary mutations deny Soul. User/facts/notes support whole-document replacement
and forgetting when the store supplies those operations. No automatic extraction,
summarization, daily-note scan, append, CAS, secure erase or Session deletion.
The application authorizes updates and serializes read-modify-write transactions.

Check both status and `agent_memory_change_t`: a replacement can be APPLIED even
when later synchronization fails. A failed removal can be UNKNOWN because the
file may already have been deleted. Neither result permits blind retry; publication
is not a power-loss guarantee. No automatic Context/ReAct integration is available.

## Build

```sh
cmake -S . -B build -DAGENT_BUILD_FILE_STORE=ON -DAGENT_BUILD_MARKDOWN_MEMORY=ON
cmake --build build
```

Link `cagent::memory_markdown`; select a File Store Port separately. ESP-IDF uses
`CONFIG_AGENT_MEMORY_MARKDOWN` with `CONFIG_AGENT_FILE_STORE`. OpenVela can reuse
the same provider target or compile this source with the common Core profile and
its File Store Port. No platform-specific Memory I/O implementation is required.

See [ADR 0032](../../../docs/zh/adr/0032-memory-domain-management.md) and the
[development guide](../../../docs/zh/development/memory.md) for API and examples.
Tests: `tests/memory/compile.sh`, `tests/providers/memory_markdown/compile.sh`,
and `tests/build/file_store/compile.sh` (Host/simulated IDF/NuttX assembly, not
native firmware or power-loss validation).
