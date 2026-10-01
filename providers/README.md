# Providers

## Model Providers

`src/model/` contains the provider-neutral wrapper and binding logic. Each optional
directory here owns a wire protocol or a test implementation, built separately from
`cagent_core`. Providers use the public `agent/model.h` and `agent/transport.h`
contracts; they do not include `src/model/model_internal.h` or platform SDK headers.

| Directory | Status | Build target |
|---|---|---|
| `model/openai/` | Synchronous non-streaming Chat Completions provider | `cagent::provider_openai`, opt-in on host CMake or ESP-IDF Kconfig |
| `model/mock/` | Placeholder for deterministic tests | `cagent_provider_mock`, opt-in on host CMake |
| `model/anthropic/` | Unimplemented native-protocol candidate | None |

The OpenAI provider can complete a request through any synchronous HTTP Transport;
see [OpenAI provider](model/openai/README.md) for buffers, lifetime, and build integration.
The Mock target remains a placeholder. An OpenAI-compatible endpoint can share the OpenAI provider.
Add a native Anthropic implementation only when its distinct protocol is needed.
Model credentials, request/response buffers, and Transport bindings belong to the
provider or application, not to the Core or `agent_model_workspace_t`.

Platform HTTP/TLS implementations remain in `ports/`. The top-level ESP-IDF
component includes OpenAI sources only with `CONFIG_AGENT_PROVIDER_OPENAI=y`.
The Provider can also be built against application-owned Transport ops on other
platforms; no SDK headers enter the Provider.

## Session Storage Providers

`storage/ram/` is an optional, volatile reference backend for the public
`agent/session.h` Storage contract. It uses caller-owned arrays and payload
buffers, has no implicit heap or filesystem dependency, and returns a capacity
error rather than deleting history when full. It does not implement power-loss
recovery. See the [Session development log](../docs/zh/development/session.md).

`storage/jsonl/` is an optional file-backed implementation of the same contract.
It accepts application-owned file callbacks and bounded buffers, so neither it
nor Core depends on POSIX or a platform SDK. See [JSONL Storage](storage/jsonl/README.md)
for its record, durability, and recovery limits.
