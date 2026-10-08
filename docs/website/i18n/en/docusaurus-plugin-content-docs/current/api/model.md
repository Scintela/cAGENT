# Model Interface

Public header: `agent/model.h`. The wrapper separates Core from vendor protocols;
the application configures a concrete Provider.

## Types

| Type | Purpose and lifetime |
|---|---|
| agent_message_role_t | Canonical SYSTEM, USER, ASSISTANT, TOOL roles |
| agent_message_view_t | Message bodies and call/result pairing, borrowed until complete returns |
| agent_tool_view_t | Model-visible name, description, Schema and flags |
| agent_model_request_t | Prompt, messages, Tools, IDs, cancel, deadline, token budget |
| agent_model_sink_t | Synchronous text/tool_call callbacks; never retain |
| agent_model_ops_t | Required complete, optional destroy |
| agent_model_workspace_t | Wrapper storage; Provider workspace is separate |

TOOL tool_call_id must match an ASSISTANT call. Wire JSON and third-party JSON
types must not enter canonical requests.

## Wrappers and Binding

| Function | Behavior |
|---|---|
| agent_model_init | Initialize caller Workspace; copy Ops, borrow context |
| agent_model_create | Allocate wrapper; NULL on failure |
| agent_model_destroy | Destroy an unbound idle wrapper; fixed storage is not freed |
| agent_set_model | Idle borrowed binding; application retains ownership |
| agent_set_model_owned | Transfer wrapper ownership only on success |
| agent_get_model | Borrow the current wrapper; not a thread-safe configuration API |

Never destroy a borrowed wrapper still bound to an Agent. Provider and wrapper
are separate objects; Provider destroy defines Provider cleanup. Concurrent
sharing needs separate design; OpenAI allows one active call per instance.
set_model does not accept NULL unbinding. Replace with another valid wrapper
before destroying the old borrowed wrapper. Binding the same wrapper again
returns EXISTS without changing ownership.

## complete Contract

- Complete every sink call before returning; no later asynchronous delivery.
- Deliver serially on the driver; stop and propagate the first sink error.
- text is UTF-8 chunks; tool_call is a complete call/object, not incremental fragments.
- Never retain input/sink/context across calls or overlap writable Provider buffers.
- Respect deadlines, timeouts and requested token budgets; verify server compliance separately.
- The sink has no usage channel; do not fabricate token statistics.

Custom Providers need not use HTTP/JSON. See [OpenAI integration](../guides/openai.md).
