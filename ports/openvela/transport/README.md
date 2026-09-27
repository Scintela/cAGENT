# OpenVela Transport

This subpackage will adapt the selected OpenVela/NuttX HTTP client or socket plus TLS stack to
`agent_transport_ops_t`. The selected stack is intentionally not frozen by Core.

Required behavior:

- complete the bounded synchronous request and stop all callbacks before returning;
- enforce response header/body limits before forwarding data to the sink;
- map link, DNS, socket, and TLS failures without converting HTTP status into transport failure;
- keep mbedTLS state, certificates, sockets, PSRAM staging buffers, and connection reuse state in
  this subpackage or in application-owned configuration.

The Model Provider owns OpenAI JSON, SSE, authorization semantics, and HTTP status interpretation.
