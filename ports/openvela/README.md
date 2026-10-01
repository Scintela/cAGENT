# OpenVela Port

This optional Port is split into [runtime/](runtime/README.md),
[transport/](transport/README.md), and [storage/](storage/README.md). It owns OpenVela/NuttX headers and build metadata; none of these
subpackages is part of the Core build.

All adapters are optional and independently selectable. Runtime provides a monotonic clock;
Transport uses `netutils/webclient` and borrowed buffers. HTTPS additionally requires an
application-provided `webclient_tls_ops` implementation that validates the server certificate chain
and hostname. V1's permissive mbedTLS handshake is deliberately not copied.
Storage binds the JSONL provider to an application-mounted filesystem and requires
target-specific durability testing before claiming power-loss recovery.

Include `ports/openvela/Kconfig` from the application's Kconfig and add
`ports/openvela` as a CMake subdirectory in a NuttX application build. Select
`AGENT_PORT_OPENVELA_RUNTIME`, `AGENT_PORT_OPENVELA_TRANSPORT`, and/or
`AGENT_PORT_OPENVELA_STORAGE`; Transport also needs `NETUTILS_WEBCLIENT` and
Storage needs the Core JSONL provider. The public headers live under each
subpackage's `include/` directory.
