# Module and Directory Map

Directories describe responsibilities, not delivery guarantees. Empty directories
or skeletons are not implemented features.

| Path | Responsibility | Entry point |
|---|---|---|
| include/agent.h | Lifecycle, Run, common application APIs | [Core](../api/core.md) |
| include/agent/ | Platform-independent domain/extension contracts | [API](../api/public-api.md) |
| src/core/ | Workspace, Arena, events, state, errors | [Memory](memory.md) |
| src/run/ | Synchronous ReAct, cooperative cancellation | [Run](run.md) |
| src/tool/, src/policy/ | Registration, admission, authorization, execution | [Tool](tool.md) |
| src/skill/ | Bounded trusted instructions registry | [Skill](skill.md) |
| src/context/ | Cross-domain model-input projection | [Context](context.md) |
| src/session/ | Current Turn, Storage forwarding/history projection | [Session](../guides/session.md) |
| src/memory/ | Long-term document commands and safety boundaries | [Memory guide](../guides/memory.md) |
| src/model/ | Wrapper/binding, no vendor protocols | [Model](../api/model.md) |
| src/runtime/, src/transport/ | Generic service/HTTP validation and forwarding | [Runtime](../api/runtime.md), [HTTP](../api/transport.md) |
| providers/model/openai/ | Non-streaming Chat Completions | [OpenAI](../guides/openai.md) |
| providers/storage/ram/, jsonl/ | Concrete Session storage | [Storage](../api/storage.md) |
| providers/storage/files/ | Shared byte-file interface | [File I/O](../api/storage.md) |
| providers/memory/markdown/ | Four logical Markdown kinds | [Memory guide](../guides/memory.md) |
| codecs/json/ | Private reader/writer | [Build](../getting-started/build.md) |
| ports/ | Runtime, HTTP and filesystem adaptation | [Platforms](../platforms/index.md) |
| tests/ | Host contracts, fault injection, build matrices | [Testing](../contributing/testing.md) |
| docs/zh/ | Source manual and maintenance records | [Quickstart](../getting-started/quickstart.md) |

## Keep These Boundaries

Codec understands JSON syntax; JSONL Provider defines Turn records. File Store
operates on bytes; Session bridge defines names and batch management. Context
coordinates sources; Memory/Skill define their meaning. Ports adapt SDKs;
src/runtime and src/transport retain generic contracts.

## Not Delivered

Official Mock/Anthropic, Host Runtime/HTTP, STM32, Skill file loader and built-in
Tools have no complete production implementation. Automatic Session-to-Memory
extraction, async confirmation and streaming are future capabilities; the current
mainline can run without them.
