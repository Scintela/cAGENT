# OpenVela Runtime

This subpackage will create an `agent_runtime_t` from OpenVela/NuttX facilities. Its source and
public header are added only with a buildable OpenVela package.

Required behavior:

- provide a nondecreasing 64-bit monotonic millisecond clock;
- document the chosen cancellation synchronization primitive and its task-context restriction;
- leave the allocator empty unless the product explicitly enables the heap convenience path;
- adapt logging synchronously without Agent re-entry.

It must not include socket, mbedTLS, HTTP, PSRAM buffer, Flash, or JSONL storage policy.
