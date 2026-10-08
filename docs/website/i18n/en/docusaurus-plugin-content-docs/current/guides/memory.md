# Memory and Markdown Documents

Session preserves original conversation facts. Memory holds application-selected
long-term content. The library does not automatically summarize/extract Session
into Memory; schedule such tasks in the application with independent budgets.

## Minimal File Layout

This is a product convention, not a hardcoded Core path:

```text
workspace/
├── SOUL.md
├── USER.md
├── MEMORY.md
└── memory/
    └── 2026-10-08.md
```

| Logical kind | Content | Writable through ordinary Memory APIs? |
|---|---|---|
| SOUL | Trusted identity and long-term behavior rules | No |
| USER | Current user profile | Yes, through trusted application commands |
| FACTS | Long-term facts/preferences in MEMORY.md | Yes |
| NOTE | Daily note identified by a valid date | Yes |

The application authorizes identity files, user roots and dates. Never accept
arbitrary model-provided paths. Use separate roots/bindings per user: Session
IDs do not automatically isolate Memory.

## Initialize the Backend

Prepare a File Store for documents and another for notes. Enable
`AGENT_BUILD_MARKDOWN_MEMORY` and link `cagent::memory_markdown`:

```c
#include <agent_markdown_memory.h>

agent_error_t attach_documents(agent_t* agent, agent_markdown_memory_t* state,
    agent_file_store_t documents, agent_file_store_t notes)
{
    const agent_markdown_memory_config_t config = {
        .soul = {
            .store = documents, .name = AGENT_SV_LITERAL("SOUL.md"),
            .max_bytes = 512u
        },
        .user = {
            .store = documents, .name = AGENT_SV_LITERAL("USER.md"),
            .max_bytes = 512u
        },
        .facts = {
            .store = documents, .name = AGENT_SV_LITERAL("MEMORY.md"),
            .max_bytes = 1024u
        },
        .notes = notes,
        .note_max_bytes = 1024u
    };
    agent_memory_t binding;
    agent_error_t status = agent_markdown_memory_init(state, &config);
    if (status == AGENT_OK)
        status = agent_markdown_memory_bind(state, &binding);
    if (status == AGENT_OK)
        status = agent_set_memory(agent, &binding);
    return status;
}
```

The fixed documents can share a trusted root; notes use an independent flat
namespace. Set unused document `max_bytes` to zero. State, Store state and path
scratch must outlive all consumers. This function does not create document content.

## Assembly Steps

1. Mount the filesystem and prepare trusted directories in the application.
2. Initialize platform File Stores with nonoverlapping path scratch.
3. Configure soul/user/facts and the independent notes root.
4. Initialize Markdown Memory and export its binding with `agent_markdown_memory_bind()`.
5. Bind it through `agent_set_memory()`.
6. Explicitly select documents through `agent_register_memory_context()`.

For example, select long-term facts:

```c
#include <agent/context.h>

agent_error_t select_memory(agent_t* agent)
{
    const agent_memory_context_t source = {
        .name = AGENT_SV_LITERAL("user-facts"),
        .key = { .kind = AGENT_MEMORY_FACTS },
        .required = false,
        .max_bytes = 512u
    };
    return agent_register_memory_context(agent, &source);
}
```

Binding Memory does not inject every file automatically. SOUL becomes
instructions; other documents become reference data. A failed required document
terminates the Turn; a failed optional document is skipped as a whole.

## Read/Write Rules

`agent_memory_read()` copies a complete document into caller storage without
silent truncation. `replace()` replaces the entire document, not a patch.
`forget()` is not secure erasure. Call these public operations only while Agent
is idle, never by reentering from Tool/Context callbacks.

Check `agent_memory_change_t` alongside status: UNCHANGED, APPLIED and UNKNOWN
describe change facts, not additional error codes. Do not blindly retry when
an error accompanies APPLIED/UNKNOWN. Update SOUL through trusted deployment,
not a normal model Tool that bypasses protection.

See [Memory](../api/memory.md). Extraction alternatives are recorded in
[ADR 0024](adr/0024-session-to-memory-extraction.md), currently in Chinese.
