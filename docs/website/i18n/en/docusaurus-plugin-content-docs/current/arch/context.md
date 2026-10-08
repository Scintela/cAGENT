# Context Architecture

Context coordinates model-visible input while domains retain original state
and invariants. Synchronous Run drives it; applications use public registration,
not private headers.

```text
Run driver
  -> Session turn transaction
  -> Context prepare
       fixed instructions + Skill selection + Memory snapshots
  -> Context project (each model call)
       dynamic callbacks + Tool views + Session history/current messages
  -> canonical Model request
       system_prompt / messages[] / tools[]
  -> Model complete
  -> release disposable projection
  -> append response facts / execute tools / next projection
```

## Files and Structures

| File | Responsibility |
|---|---|
| context_registry.c | Shared fixed registry for dynamic sources/Memory selections |
| context_builder.c | Callbacks, cancel/deadline, sticky sink, complete UTF-8 checks |
| context_projection.c | Turn snapshots, cross-domain budgets, input assembly, LIFO release |
| context_internal.h | Private state, sources, skip report and driver APIs |

`agent_context_turn_t` lives in scratch and holds Agent, copied request/effective
limits, cancel/deadline, static views, Memory snapshots and initial report.
`agent_context_projection_t` is driver-held outside scratch, with model request,
mark/end and report. Reports contain bounded names/statuses, not full private text.

## Driver Contract

1. Run starts Session in the same Agent scratch and enters ACTIVE.
2. `agent_context_prepare()` prechecks required bounds, selects Skills and reads Memory; failure rewinds its mark.
3. Before per-call projection, Run reserves result storage that must survive Model return.
4. `agent_context_project()` builds one input; remaining_tool_calls=0 omits Tools.
5. After synchronous Model return, release rewinds the call mark before Session appends.
6. Later calls reuse snapshots but refresh dynamic data/messages; Run clears Turn scratch at the end.

Project verifies transaction Arena, Session ID and initial input to avoid mixing
requests. Empty IDs normalize to default and the projection uses that actual ID.
Effective limits, trace, cancel, requested tokens and remaining timeout reach Model.

Release is LIFO: new allocations after projection cause STATE. Model sink must
write into earlier reservations, not append persistent facts at the projection
tail. reserve_bytes restricts available tail space but does not manage Run results.

## Data and Trust

System text order is fixed prompt, selected Skills, SOUL and dynamic INSTRUCTIONS,
with stable priority within classes. Other Memory/REFERENCE messages precede
historical and current Session messages. References are never written back.

Tools remain structured views selected by visibility; guards still authorize
execution. Providers encode canonical input; Context builds no tools_json or
messages_json. Content partitions are not sandboxes. Trust sources and enforce
Policy/device authorization in the application.

## Memory and Failure Boundaries

Registries occupy persistent storage; transactions/snapshots/projections use
scratch with no heap. Reserve required declared bounds first. Optional blocks
are whole-or-skip. Collection order differs from stable presentation order.

Allocate message/Tool descriptors before history replay and reserve current and
required-reference slots. Session reads complete groups only in remaining space,
maintains pairing and checks cancel/deadline per group. Storage callbacks use
Agent callback guards; blocking I/O still needs backend bounds.

Text includes bodies and conservative separators; history uses remaining scratch.
Optional snapshots also conservatively reserve descriptors, instruction text
and required dynamic output. This is not exact token counting or a guarantee
that every profile/workload fits.

Dynamic output is collected before final system-text allocation, so instruction
bytes may temporarily exist twice. Peaks include copying/alignment and failed
builds; rewind does not reduce high-water marks. Local Arena descriptors publish
used only on success, preserving transaction/snapshot state on failure. Core
no-heap does not constrain callbacks, Storage or HTTP/TLS SDKs.

## Verification and Limits

Tests cover snapshots, repeated projection/release, dynamic refresh, split UTF-8,
ignored sink errors, required/optional failures, cancel/deadline, sorting, RAM
history and zero-slot builds. Host/fake-IDF link tests verify disabling Tools
removes their JSON dependency; sanitizer checks have passed.

On-demand/file-loaded Skills, Memory retrieval/summarization, token estimation,
public skip events and real-MCU stack/peak measurements remain outside this
module's delivery. Run integration is described in [execution architecture](run.md).
