# Tool Architecture

Tool registration, admission, authorization and execution are integrated with
ReAct, Session and Events. See the [public API](../api/tool.md) and
[Chinese development record](development/tool.md). Current implementation
does not require descriptor registration or Schema caches, and provides no
confirmation pause, automatic retry or parallel execution.

## 1. Files and Responsibilities

| Location | Responsibility |
|---|---|
| include/agent/tool.h, policy.h | Definitions/callbacks/registration and one product Policy |
| include/agent/types.h, model.h | Shared calls/sinks/limits and Model Tool views |
| src/core/lifecycle.c | Registry/scratch reservation in Workspace |
| src/tool/tool_internal.h | Private registry, projection, invocation and execution facts |
| src/tool/tool_registry.c | Slots, borrowing, enumeration, visible projection |
| src/tool/tool_schema.c | Text/object admission, not JSON Schema semantics |
| src/tool/tool_guard.c | Cancel/deadline, validate, Policy, handler, output |
| src/policy/policy_chain.c | Single Policy and fail-closed decisions |
| src/run/ | Iterations, budgets, Session/Event/statistics |
| codecs/json/reader.c | Strict JSON/UTF-8, token-free unique-key object checks |
| codecs/json/writer.c | Provider formatting; not a Tool dependency |
| providers/model/openai/ | Canonical Tool views to OpenAI JSON |
| tests/tool/, tests/build/tool/ | Runtime/profile contracts and Host/fake-IDF linking |

Tools contain no SDK, HTTP, file format, vendor protocol or smart-home device
implementation. Handlers may call external backends and own their memory and
idempotency. Core Tool allocates no heap. Caller Workspace may be static or
allocated before init; optional agent_create allocation is not per-Tool heap use.

## 2. Memory and Lifetimes

```text
Agent Workspace
  Persistent
    agent_t
    agent_tool_registry_t: count + entries[AGENT_MAX_TOOLS]
      Copied definitions; externally borrowed strings and user_data
  Turn scratch
    Visible agent_tool_view_t[]
    Run-retained call arguments/IDs
    Per-call output buffer and execution facts

Application
  Static/addon-owned text and Schema
  Device callback state
  Model Provider request/response buffers
```

Init allocates/zeros registry; zero slots means NULL and no allocation. Register
validates everything before committing count. Unregister compacts/clears tail
without reordering others. No owned-Tool mode, deep-copy pool or realloc.

Projection copies visible name/description/Schema/flags views and prechecks
capacity. Failure publishes no partial array and returns count=0/CAPACITY.
Reject text overlap, arithmetic overflow and array/count aliasing. Views remain
valid only while registry/text remain unchanged; Run freezes registration for
their use and never retains entry pointers across removal/reconfiguration.

## 3. Invocation Flow

```text
Application registration
 -> Idle/reentry and metadata/flags/Schema admission
 -> Persistent registry -> Visible Model views
Run -> Model -> Provider -> tools JSON -> HTTP -> Model
 -> Complete call sink -> Copy ID/name/arguments
 -> Reserve output; pass remaining handler budget
Private agent_tool_invoke
 -> ACTIVE, alias and output-space checks
 -> Lookup, visibility, IDs
 -> Effective deadline, cancel, budget
 -> Argument object -> Pure validate -> Explicit Policy ALLOW
 -> Recheck cancel/deadline -> Handler once -> Bounded sticky sink
 -> Validate text -> Execution facts
Run
 -> Consume budget when handler_called
 -> Pair results; update events/statistics
 -> Next iteration or finalize Turn
```

Run/Session own call arrays/IDs, pairing, persistence and event ordering. Tool
does not append history. No public bypass exists; never include private headers
or mutate Agent state manually.

## 4. Independent Checks

1. Structural admission: lengths, complete objects, UTF-8, escapes, numbers, depth, unique keys.
2. Business semantics: types/ranges/unknown fields/device preconditions in validate/handler.
3. Authorization: trusted product identity/source/device state in Policy; Schema/READ_ONLY grants nothing.

Schema and arguments share admission logic with different byte bounds. No token
array is built: strict syntax precedes decoded-key comparisons, including nested
and escape-equivalent duplicates; NUL keys fail. Generic JSON parse behavior is
not silently changed.

Bounded rescanning avoids a maximum token array but can take quadratic work;
stack grows with depth, not member count. Measure CPU/stack when raising bounds.
Depth 32 is a grammar ceiling, not a guarantee that every MCU task stack fits.

## 5. Execution Facts and Output

Private `agent_tool_execution_t` is distinct from public response:

| Field | Meaning |
|---|---|
| status | Final admission/cancel/deadline/output/handler outcome |
| handler_called | Set before calling handler; external effects may have occurred |
| handler_status | Normalized handler result; NOT_SUPPORTED before invocation, meaningful only when called |
| output_status | Sticky sink/final UTF-8/NUL status |
| output | Borrowed destination; partial diagnostics on failure are not successful results |

Alias/pointer/reentry precheck failures do not write results. Later failures
initialize empty output/called=false. After handler return precedence is current
cancel/timeout, then output error, then handler error; separate fields retain
their own facts. Failed validation/Policy is not called repeatedly.

Reserve complete output capacity before side effects. Writes accept/reject whole
chunks, never silently truncate, and stop at first error. Final UTF-8 may span
chunks. Partial diagnostics cannot prove device success or justify retries.

## 6. Lifecycle Boundaries

- Mutations/Policy settings require CONFIGURING/READY; ACTIVE returns BUSY.
- Driver-only queries/enumeration are callback-guarded, not thread-safe.
- Business callbacks do not reenter Agent; supported cross-task cooperative cancellation follows its separate synchronization contract.
- Tool deadline covers validation, Policy and handler; tighter overall deadline wins. Boundaries/writes poll; blocked callbacks delay observation.
- Missing/invalid/CONFIRM Policy and REQUIRES_CONFIRM fail closed, without hidden UI state.
- Run consumes handler budget whenever handler_called, including failure. Tool runs once and maintains no Turn-global counter.

## 7. Build Boundaries

Enabled Core Tools link cagent_json_reader, not the full writer. Full
cagent_json_jsmn adds writer and reuses that reader. OpenAI/JSONL do not compile
duplicate readers; ESP-IDF collects sources under equivalent conditions.

Zero Tool capacity removes the Tool reader dependency, but independent Providers
may still need JSON. Public headers expose no third-party JSON types; jsmn
symbols remain translation-unit isolated.
