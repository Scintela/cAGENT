# ESP-IDF Port

This optional Port is split into [runtime/](runtime/README.md) and
[transport/](transport/README.md). It owns ESP-IDF headers and component metadata; neither
subpackage is part of the Core build.

Both subpackages now have source; public headers are shared under `include/` as
`agent_espidf_runtime.h` and `agent_espidf_transport.h`. They are validated with a C99 mock SDK; an
ESP-IDF hardware integration target is still required before claiming a supported SDK release.

For an ESP-IDF application, expose the Core component as `components/cagent` and add this directory
to the component search path. The Port component requires the Core component named `cagent`;
Kconfig independently controls its Runtime and Transport sources and SDK dependencies.
