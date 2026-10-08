# Architecture

cAgentV2 separates execution mechanisms, domain protocols and platform services.
The application chooses dependencies, budgets and trust; Core coordinates the
order of operations and invariants within a Turn.

## Layers

```text
Application
  Configuration / Workspace / authorization / task queues / mounts and networking
  |
  +-- Core (src/)
  |   lifecycle / arena / event / cancel
  |   run -> context -> model contract
  |       -> tool -> policy
  |       -> session contract
  |       -> memory contract
  |   skill and context registries
  |
  +-- Optional Providers (providers/)
  |   model/openai      Canonical requests <-> Chat Completions
  |   storage/ram       Volatile Session storage
  |   storage/jsonl     Turns <-> JSONL
  |   storage/files     Byte-file contract and bounded helpers
  |   memory/markdown   Logical documents <-> Markdown files
  |
  +-- Private Codec (codecs/json/)
  |   Strict JSON reader / bounded writer / embedded jsmn
  |
  +-- Platform Ports (ports/)
      espidf / openvela / rtthread
        runtime / transport / storage
      posix/storage     Reusable physical byte I/O
```

Vendor wire JSON stays outside Core, file paths stay outside Session/Memory
contracts, and SDK types stay outside generic public headers. Enabling an
optional package does not initialize its resources.

## Execution Flow

```text
agent_run(request)
 -> Admission, effective limits and deadline
 -> Current Session Turn / Storage transaction
 -> Prepare Skill and Memory snapshots
 -> Project system_prompt / messages[] / tools[]
 -> Model.complete
      -> OpenAI JSON -> HTTP Transport -> response JSON -> Model sink
 -> Copy model results into Core storage
      +-- final: finish
      +-- tool_calls: arguments / Policy / handler / paired results -> project again
 -> Session finish
 -> Text delivery, statistics, TURN_END
 -> Rewind scratch and return to READY
```

This synchronous chain is implemented. There is no public begin/step/resume/end
API, Plugin mount/unmount, background Agent thread or automatic memory extraction.

## Responsibilities

| Domain | Owns | Does not provide |
|---|---|---|
| Run | Turn control, budgets, cancellation, terminal outcome | Product scheduling, UI, reconnect logic |
| Context | Bounded model-input projection | Persistence or vendor serialization |
| Tool/Policy | Registration, validation, authorization, synchronous execution | Automatic device rollback or a generic sandbox |
| Skill | Registration and selection of trusted full-text instructions | File scanning or code execution |
| Session | Current-Turn pairing and historical group projection | Mandatory JSONL/NVS storage |
| Memory | Logical long-term documents and change semantics | Automatic summarization or vector retrieval |
| Provider | Protocols, formats and independent working buffers | Platform HTTP/TLS implementations |
| Port | SDK services, networking and byte I/O | JSONL/Markdown domain rules |

## Lifecycle and Memory

An instance moves through CONFIGURING -> READY -> ACTIVE -> READY. Core Workspace
holds persistent state and registries plus reusable Turn scratch. Model wrappers,
Providers, Storage, HTTP/TLS and task stacks have separate budgets.

Fixed-storage Core initialization and execution do not allocate heap memory.
Optional create wrappers and SDKs may allocate. Views avoid unnecessary copies,
but callback results, selected history and snapshots still need copies: no heap
does not mean zero-copy.

Continue with the [module map](arch/module-map.md), [Run](arch/run.md),
[memory model](arch/memory.md), [Tool](arch/tool.md), [Skill](arch/skill.md) and
[Context](arch/context.md). [ADRs](adr/index.md) preserve historical alternatives,
not necessarily implemented APIs.
