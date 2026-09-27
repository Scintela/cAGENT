# OpenVela Port

This optional Port is split into [runtime/](runtime/README.md) and
[transport/](transport/README.md). It owns OpenVela/NuttX headers and build metadata; neither
subpackage is part of the Core build.

Both adapters are optional and independently selectable. Runtime provides a monotonic clock;
Transport uses `netutils/webclient` and borrowed buffers. HTTPS additionally requires an
application-provided `webclient_tls_ops` implementation that validates the server certificate chain
and hostname. V1's permissive mbedTLS handshake is deliberately not copied.

Include `ports/openvela/Kconfig` from the application's Kconfig and add
`ports/openvela` as a CMake subdirectory in a NuttX application build. Select
`AGENT_PORT_OPENVELA_RUNTIME` and/or `AGENT_PORT_OPENVELA_TRANSPORT`; the latter also needs
`NETUTILS_WEBCLIENT`. The public headers live under each subpackage's `include/` directory.
