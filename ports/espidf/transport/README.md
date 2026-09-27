# ESP-IDF Transport

This subpackage adapts `esp_http_client` to the synchronous `agent_transport_ops_t` contract. Its
source is in `src/`; the shared Port header is `ports/espidf/include/agent_espidf_transport.h`.
The component build selects the source explicitly.

Required behavior:

- accept a borrowed bounded `agent_http_request_t` and finish all callbacks before `request()` returns;
- emit final headers once, then decoded body chunks, while enforcing both response byte ceilings;
- map DNS, socket, and TLS failures to Agent transport errors without interpreting HTTP status;
- keep ESP-IDF client state, certificates, connection pools, and buffers outside Core workspace.

The Model Provider, not this adapter, interprets HTTP `401`, `429`, and `5xx` responses.

The adapter creates and cleans up one ESP client per request. Its state is caller-owned and rejects
recursive use, but it is not a cross-task shared connection pool. ESP-IDF internal allocations are
outside the Core zero-heap guarantee.

For HTTPS, set exactly one trust source in `agent_port_espidf_transport_config_t`:

- `cert_pem`: a borrowed, NUL-terminated PEM CA certificate;
- `use_crt_bundle`: the ESP-IDF certificate bundle, available only when
  `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE` is enabled. In that build, the Port privately depends on
  `mbedtls`.

Requests without a trust source are rejected before `esp_http_client_init()`. HTTP remains available
for applications that explicitly request it. The adapter does not disable hostname validation or
follow redirects. Its synchronous `esp_http_client_perform()` call may defer observing cancellation
until the SDK returns or invokes an event callback; test deadline and TLS behavior on the target SDK.
