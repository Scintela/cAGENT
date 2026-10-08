# Sessions and History

A Session is a conversation identified by `session_id`. A Turn is one user
request and its model/Tool iterations. Turns, model iterations and file lines
are distinct concepts.

## Choose Storage

| Scenario | Approach |
|---|---|
| One request, no history | Do not bind Storage |
| Tests or volatile conversations | `providers/storage/ram`, with caller arrays and payload |
| Persistent files | JSONL Provider + File Store bridge + platform backend |
| NVS/Flash or remote service | Implement `agent_session_storage_ops_t`, retaining complete-group semantics |

Core knows Storage Ops, not paths, JSONL, NVS or mounts. RAM is optional, not a
mandatory intermediate cache.

## Bind RAM Storage

Enable `AGENT_BUILD_SESSION_RAM` and link `cagent::session_ram`. This is an
example instance budget, not a universal product default:

```c
#include <agent_session_ram.h>

typedef struct {
    agent_session_ram_t backend;
    agent_session_ram_turn_t turns[4];
    agent_session_ram_message_t messages[32];
    agent_session_ram_call_t calls[8];
    char payload[8192];
    agent_message_view_t read_messages[32];
    agent_tool_call_view_t read_calls[8];
} app_history_t;

agent_error_t attach_ram_history(agent_t* agent, app_history_t* state)
{
    const agent_session_ram_config_t config = {
        .turns = state->turns, .turn_capacity = 4u,
        .messages = state->messages, .message_capacity = 32u,
        .calls = state->calls, .call_capacity = 8u,
        .payload = state->payload, .payload_capacity = sizeof(state->payload),
        .read_messages = state->read_messages, .read_message_capacity = 32u,
        .read_calls = state->read_calls, .read_call_capacity = 8u
    };
    agent_session_storage_t storage;
    agent_error_t status = agent_session_ram_init(&state->backend, &config);
    if (status == AGENT_OK)
        status = agent_session_ram_bind(&state->backend, &storage);
    if (status == AGENT_OK)
        status = agent_set_session_storage(agent, &storage);
    return status;
}
```

Keep state in application storage with sufficient lifetime. Arrays and payload
can independently fill up. The backend neither grows indefinitely nor rotates
automatically; the application chooses cleanup while idle.

## Bind JSONL Storage

Enable the full JSON codec, JSONL, File Store and selected platform backend.
Link JSONL and its file bridge. The application initializes the platform Store.
All Stores and buffers used below must outlive their consumers:

```c
#include <agent_session_jsonl_files.h>

typedef struct {
    agent_session_jsonl_t backend;
    agent_session_jsonl_files_t files;
    char name[256];
    char write_line[16384];
    char read_line[16384];
    char decoded[12288];
    /* The public contract requires int alignment, not a private token type. */
    int tokens[2048];
    agent_message_view_t messages[32];
    agent_tool_call_view_t calls[16];
    char session_id[65];
} app_jsonl_history_t;

agent_error_t attach_jsonl_history(agent_t* agent, app_jsonl_history_t* state,
    const agent_file_store_t* store)
{
    agent_session_jsonl_config_t config = {
        .write_line = state->write_line,
        .write_line_capacity = sizeof(state->write_line),
        .read_line = state->read_line,
        .read_line_capacity = sizeof(state->read_line),
        .decoded = state->decoded, .decoded_capacity = sizeof(state->decoded),
        .token_buffer = state->tokens, .token_buffer_bytes = sizeof(state->tokens),
        .read_messages = state->messages, .read_message_capacity = 32u,
        .read_calls = state->calls, .read_call_capacity = 16u,
        .session_id = state->session_id,
        .session_id_capacity = sizeof(state->session_id)
    };
    agent_session_storage_t storage;
    agent_error_t status = agent_session_jsonl_files_init(&state->files, store,
        state->name, sizeof(state->name), &config);
    if (status == AGENT_OK)
        status = agent_session_jsonl_init(&state->backend, &config);
    if (status == AGENT_OK)
        status = agent_session_jsonl_bind(&state->backend, &storage);
    if (status == AGENT_OK)
        status = agent_set_session_storage(agent, &storage);
    return status;
}
```

A JSONL workspace may reasonably exceed Core size: it encodes/decodes a complete
Turn and is outside the 32 KiB Core budget. Example buffers do not guarantee
encoding the largest profile; validate worst-case group sizes. Place suitable
backend storage in application-selected PSRAM, or lower physical limits.
The bridge converts Session IDs to hexadecimal filenames. Name capacity must
also include prefixes, suffixes and a terminator, not just the original ID length.

## Requests and History Windows

```c
#include <agent.h>

agent_error_t ask_with_history(agent_t* agent, agent_response_t* response)
{
    agent_request_t request = {
        .session_id = AGENT_SV_LITERAL("living-room"),
        .input = AGENT_SV_LITERAL("What did we discuss last time?")
    };
    agent_limits_t limits = AGENT_LIMITS_DEFAULT;
    limits.max_history_turns = 3u;
    request.limits = &limits;
    /* The caller initializes response.output and response.output_size. */
    return agent_run(agent, &request, response);
}
```

The default `max_history_turns=0` disables history projection, not recording of
new Turns by bound Storage. Non-NULL request limits replace the entire limits
structure, not just nonzero fields.

`recent()` yields the most recent complete Turn groups, newest first. Core
selects a bounded window and presents it chronologically. It does not break
Tool call/result pairs to fit the budget.

## Write and Read Flow

```text
Request -> Current Turn in Core scratch
        -> Storage begin / append
        -> Append model and Tool iterations
        -> finish(COMPLETE or ABORTED)
        -> Encode JSONL record, append file, sync

Later request -> recent(bounded candidates)
              -> Decode one complete group in Provider
              -> Copy selected history into Core scratch
              -> Context projection
```

JSONL currently stores one versioned Turn object per line at finalization. It is
neither one event per line nor a durable pre-operation execution journal. It
cannot guarantee exactly-once physical actions after power loss.

## Failures and Retention

Check `response.session_status`. Storage failure does not mean device actions
did not happen; failed `sync()` does not prove the record was not written. Do
not blindly rerun the whole Turn.

JSONL handles unterminated tail lines with read/repair rules, not a substitute
for filesystem power-loss tests. Retention, rotation, compression, encryption
and Flash wear belong to Storage/application. `max_history_turns` controls only
the context window and deletes no history.

See [Storage Providers](../api/storage.md) and the [Session contract](../api/session.md).
