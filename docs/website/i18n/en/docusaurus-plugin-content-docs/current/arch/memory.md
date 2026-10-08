# Memory Model

The application chooses physical budgets; the library allocates within those
domains. Not everything belongs to one Arena or lives in Core.

```text
Application
  +-- Core Workspace
  |   +-- Persistent: agent_t, registries, bindings, statistics
  |   +-- Turn scratch: snapshots, messages, views, model output, Tool results
  +-- Model wrapper Workspace
  +-- Model Provider: request / response / decode / tokens
  +-- Session Storage: RAM payload or JSONL codec buffers
  +-- File Store path scratch
  +-- HTTP/TLS SDK state and buffers
  +-- Application stacks and device resources
```

## Four Capacity Concepts

| Kind | Example | Does it reserve body bytes? |
|---|---|---|
| Persistent capacity | Tool/Skill/Context slots | Metadata reserved; text usually borrowed |
| Turn scratch budget | AGENT_SCRATCH_BYTES | One reusable temporary region |
| Per-object admission cap | Schema/arguments/output bound | Admission, not independent arrays per object |
| Workspace total | AGENT_CORE_WORKSPACE_BYTES | Caller storage; internal layout compile-checked |

Scratch uses lifetime-aware LIFO allocation, not a fragmented general heap.
Live Turn output/facts cannot be overwritten; disposable model projections can
be reclaimed. Larger Context can consume remaining scratch only, not persistent
storage or exceed per-object caps.

## Distinct Concepts

- No heap: no dynamic allocator calls, but Workspace allocation/copy/reclaim still occur.
- Zero-copy views: borrowing does not extend source-buffer lifetime.
- PSRAM: application-selected placement, not automatic Core detection of SRAM/PSRAM/DMA/TLS suitability.

Suitable Provider/history buffers may live in PSRAM after checking alignment,
access and SDK constraints. Keep large Workspaces off small task stacks.

## Tuning

Measure summary.scratch_peak_bytes, then independent Provider/Storage/SDK peaks.
Increasing total Workspace does not increase scratch or OpenAI response capacity.
Increasing object caps does not supply total budget.

Test complete history windows, largest Schemas, iterations, failure paths and
JSON-depth stack usage. There is no whole-stack no-heap guarantee or unified heap
high-water statistic. See [configuration defaults](../api/config.md).
