# Ports

`ports/` contains optional platform packages. The Core library never compiles this directory
implicitly and has no platform-selection preprocessor logic.

Each package may provide either or both of these independent adapters:

- Runtime: populates `agent_runtime_t` with a monotonic clock and optional short cancel sync or log callback.
- Transport: implements `agent_transport_ops_t` using the platform HTTP/TLS stack.

Port state, SDK headers, Kconfig/CMake metadata, network buffers, TLS state, and connection pools
remain inside the selected package. They never enter `src/` or `agent_workspace_t`.

Target package layout:

```text
ports/<platform>/
  README.md
  runtime/                        # Optional Runtime subpackage.
  transport/                      # Optional HTTP/TLS Transport subpackage.
```

Each subpackage owns its own `include/`, `src/`, `tests/`, and platform-native build metadata when
it gains a real implementation. This permits a Runtime-only Port on a networkless device and a
Transport-only Adapter that uses application-provided Runtime callbacks.

The current directories are ownership and delivery boundaries, not claims that an adapter is
implemented. An application may always construct `agent_runtime_t` or `agent_transport_t` itself.
