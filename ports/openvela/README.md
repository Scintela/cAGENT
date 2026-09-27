# OpenVela Port

This optional Port is split into [runtime/](runtime/README.md) and
[transport/](transport/README.md). It owns OpenVela/NuttX headers and build metadata; neither
subpackage is part of the Core build.

The `netutils/webclient` Transport adapter is implemented and optional. A Runtime builder is not
shipped yet; applications supply `agent_runtime_t` (at least a monotonic clock). HTTPS additionally
requires an application-provided authenticated `webclient_tls_ops` implementation.
