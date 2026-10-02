# ESP-IDF Port

This optional Port is split into [runtime/](runtime/README.md),
[transport/](transport/README.md), and [storage/](storage/README.md). It owns ESP-IDF headers and component metadata; none of them
subpackage is part of the Core build.

Each subpackage owns its source and include directory. Its public headers include
`runtime/include/agent_espidf_runtime.h`, `transport/include/agent_espidf_transport.h`,
and `storage/include/agent_espidf_file_store.h` (legacy Session binding remains available).
They are validated with a C99 mock SDK; an
ESP-IDF hardware integration target is still required before claiming a supported SDK release.
Storage additionally needs target-filesystem durability testing; Host tests alone cannot
establish power-loss guarantees.

For an ESP-IDF application, expose the Core component as `components/cagent` and add this directory
to the component search path. The Port component requires the Core component named `cagent`;
Kconfig independently controls Runtime, Transport and file sources and SDK dependencies.
Shared byte-file access is separate from JSONL and can serve USER/Memory reads;
see the storage README for required filesystem capabilities and the no-symlink profile.
