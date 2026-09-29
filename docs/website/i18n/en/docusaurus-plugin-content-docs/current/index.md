# cAgentV2

cAgentV2 is a cross-platform C library for building agents on embedded systems
and hosts. It targets ESP-IDF, OpenVela, RT-Thread and Host environments that
share one application API. The project is organized as a platform-independent
Core, optional Model Providers, and platform Ports; capacities are fixed at
build time, while instance resources and dependencies are supplied at
initialization time.

> **Still in an early implementation stage**: Core workspace, lifecycle,
> Runtime/Transport contracts, ESP-IDF/OpenVela adapters and the private JSON
> codec have code and host tests; the synchronous `agent_run()` does not yet
> implement the ReAct loop, and the OpenAI/Mock providers and session storage
> remain placeholders. The library cannot yet hold an end-to-end LLM
> conversation.

> **Note on translations**: the English site is under construction. Pages that
> have not been translated yet keep their Chinese originals under the same URL
> (Architecture, the ADR section, and the API reference). Use the language
> switcher in the top-right corner to move between locales.

## Architecture

```text
Application
  |  build configuration, workspace, tools, model and platform dependencies
  v
Public API (include/agent.h, include/agent/*.h)
  |
  +-- Core (src/)                 lifecycle, run contracts, registration, resource bounds
  |     +-- Model contract        calls provider ops; no vendor protocols here
  |     +-- Runtime contract      monotonic clock, optional platform services
  |     +-- Transport contract    HTTP request/response and streaming interfaces
  |
  +-- Model Providers (providers/)  OpenAI-style protocols, opt-in at build time
  |     +-- JSON codec (codecs/json/)  private bounded reader/writer, opt-in
  |     +-- Transport ops
  |
  +-- Platform Ports (ports/)     runtime and HTTP/TLS platform implementations
```

The diagram shows module responsibilities, not implemented execution paths.
The Core never includes platform SDKs, HTTP/TLS implementations or OpenAI JSON
formats directly. Applications pick the components they need at build time,
inject a Runtime through `agent_config_t`, bind a Model with `agent_set_model()`,
and network-facing Model Providers own their own Transport.

## Resources and configuration

- Core capacities are fixed at build time via CMake variables, optional
  Kconfig, or unified C configuration macros; Kconfig is not required for a
  plain CMake build.
- `agent_init()` uses an application-provided `agent_workspace_t`;
  `agent_create()` can allocate the same workspace through the runtime
  allocator. The Core places instance state and per-turn reusable scratch
  inside it. Model Provider and platform Transport state and buffers are not
  part of the Core workspace and should be budgeted separately.
- `agent_config_t` carries the instance Runtime, default limits and system
  prompt; limits constrain execution behavior but never enlarge the physical
  capacities fixed at build time.
- `codecs/json/` takes input, a token array and output buffers from the caller
  and never calls the heap allocator. This does not imply that Transports, TLS
  or the whole application are heap-free.

## Building

The platform-independent Core builds with plain CMake:

```sh
cmake -S . -B build
cmake --build build
```

Override build-time capacities or build the JSON codec separately as needed:

```sh
cmake -S . -B build -DCONFIG_AGENT_MAX_TOOLS=16 -DAGENT_BUILD_JSON_CODEC=ON
cmake --build build
```

A plain CMake build does not compile platform Ports or Providers. On ESP-IDF
the Core and Port components are pulled into the application build explicitly;
the OpenVela port uses its NuttX build entry.

## Verification

```sh
bash tests/headers/compile.sh
bash tests/core/compile.sh
bash tests/json/compile.sh
bash tests/transport/compile.sh
bash tests/ports/espidf/compile.sh
bash tests/ports/openvela/compile.sh
```

These scripts verify the current contracts with a host compiler and simulated
platform headers; the JSON tests additionally check link coexistence with an
application-provided jsmn. They do not replace on-device network, TLS, stack
and peak-memory testing.

## License

This project is released under the MIT License.
