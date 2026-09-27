# ESP-IDF Runtime

This subpackage creates a minimal `agent_runtime_t` from ESP-IDF services. Its source is in `src/`
and its public header is `include/agent_espidf_runtime.h`. The component build selects the source
explicitly.

Required behavior:

- use `esp_timer_get_time() / 1000` as a 64-bit monotonic clock;
- keep the allocator empty by default so caller-workspace use has no implicit heap path;
- use a short FreeRTOS critical section only when cancellation synchronization is enabled;
- adapt logging without re-entering the Agent or retaining callback-lifetime text.

It must not initialize Wi-Fi, start a task, configure HTTP/TLS, or own network buffers.
