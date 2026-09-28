# Model Providers

`src/model/` contains the provider-neutral wrapper and binding logic. Each optional
directory here owns a wire protocol or a test implementation, built separately from
`cagent_core`. Providers use the public `agent/model.h` and `agent/transport.h`
contracts; they do not include `src/model/model_internal.h` or platform SDK headers.

| Directory | Status | Build target |
|---|---|---|
| `openai/` | Placeholder for an OpenAI-compatible HTTP/JSON provider | `cagent_provider_openai`, opt-in on host CMake |
| `mock/` | Placeholder for deterministic tests | `cagent_provider_mock`, opt-in on host CMake |
| `anthropic/` | Unimplemented native-protocol candidate | None |

The current optional targets compile placeholder translation units; they do not
provide a usable Model. An OpenAI-compatible endpoint can share the OpenAI provider.
Add a native Anthropic implementation only when its distinct protocol is needed.
Model credentials, request/response buffers, and Transport bindings belong to the
provider or application, not to the Core or `agent_model_workspace_t`.

Platform HTTP/TLS implementations remain in `ports/`. ESP-IDF component packaging
for providers is not implemented yet; the top-level ESP-IDF component builds Core
only. Do not enable an empty provider target as a substitute for a real backend.
