# ESP-IDF Transport

This subpackage adapts `esp_http_client` to the synchronous `agent_transport_ops_t` contract. Its
public header and source are in `include/` and `src/`; the product's ESP-IDF component build
selects them explicitly.

Required behavior:

- accept a borrowed bounded `agent_http_request_t` and finish all callbacks before `request()` returns;
- emit final headers once, then decoded body chunks, while enforcing both response byte ceilings;
- map DNS, socket, and TLS failures to Agent transport errors without interpreting HTTP status;
- keep ESP-IDF client state, certificates, connection pools, and buffers outside Core workspace.

The Model Provider, not this adapter, interprets HTTP `401`, `429`, and `5xx` responses.

The adapter creates and cleans up one ESP client per request. Its state is caller-owned and rejects
recursive use, but it is not a cross-task shared connection pool. ESP-IDF internal allocations are
outside the Core zero-heap guarantee.
