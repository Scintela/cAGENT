# OpenVela / NuttX Integration

`ports/openvela/` exposes `agent_openvela_runtime.h`,
`agent_openvela_transport.h` and `agent_openvela_file_store.h`.

## Build Integration

Include Core, Providers and Ports in the NuttX application build. Source the root
and Port Kconfig, then add `ports/openvela` in the NuttX CMake environment. It
uses nuttx_add_library, not a replacement for ordinary Host CMake.

| Switch | Prerequisite |
|---|---|
| CONFIG_AGENT_PORT_OPENVELA_RUNTIME | Monotonic clock |
| CONFIG_AGENT_PORT_OPENVELA_TRANSPORT | CONFIG_NETUTILS_WEBCLIENT |
| CONFIG_AGENT_PORT_OPENVELA_FILE_STORE | CONFIG_AGENT_FILE_STORE and mounted POSIX filesystem |

All targets must share configuration and link Core/Providers/network libraries.
Reuse existing File Store/POSIX targets rather than compile duplicate backends.

## Service Binding

`agent_port_openvela_runtime_init()` supplies a clock-only Runtime. Add allocator,
logging or cross-task cancellation synchronization as needed.

Transport config includes Runtime, URL/request-header buffers/arrays, webclient
I/O buffer and response-header descriptors/text storage.
`agent_port_openvela_transport_init()` validates and exports HTTP Ops without
opening a connection.

HTTPS needs authenticated `webclient_tls_ops` and tls_context that verify both
chain and hostname. Having webclient does not imply TLS is configured; never
copy permissive handshakes as a compatibility workaround.
request_timeout_ms mainly bounds individual I/O idle time. Overall deadline is
cooperative and cannot guarantee preemption of blocking DNS, handshake or I/O.

## File Store

`agent_port_openvela_file_store_init()` binds an application-mounted trusted
root and reuses byte I/O, without requiring JSONL. JSONL and Markdown consume
that contract independently; Port contains no Session format.

## Verification

```sh
bash tests/ports/openvela/compile.sh
bash tests/build/file_store/compile.sh
```

Fake SDK tests are not a complete OpenVela firmware build. Validate the target
toolchain, TLS components, filesystem capabilities, reboot replay and power loss
separately.
