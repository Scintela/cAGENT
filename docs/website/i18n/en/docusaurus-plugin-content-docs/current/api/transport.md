# HTTP Transport Interface

Header: `agent/transport.h`. Unified `agent_transport_ops_t` currently has only
synchronous request; `agent_transport_t` borrows Ops/context. Model Provider
holds Transport, not Core config.

## Requests and Reception

| Type | Fields |
|---|---|
| agent_http_header_t | Length-bounded name/value, without CR/LF/NUL |
| agent_http_request_t | Method, absolute URL, headers, body, response byte/header budgets, deadline, cancel |
| agent_http_sink_t | Synchronous headers/body callbacks and context |

Body is bytes, not necessarily UTF-8. Response body/header budgets must be
nonzero. Request descriptors, inputs and sink state remain borrowed until return.

## Backend Contract

1. Complete all callbacks before return; do not silently make this API asynchronous.
2. Deliver final headers once before body; failures without a response need not fabricate headers.
3. Body chunks exclude HTTP transfer framing; they need not align with JSON/SSE frames.
4. Stop and propagate the first sink error; stop touching receive buffers.
5. AGENT_OK means exchange completion, not automatically HTTP 2xx.
6. Fail on body/header budget overflow; never silently discard data.
7. Check cancellation/deadlines; interruption of blocked SDK phases is backend-dependent.
8. HTTPS verifies certificate chain and hostname, without insecure fallback.

Model HTTP-status mapping and SSE/JSON parsing belong to protocol Providers,
not Ports. Backends manage I/O, TLS, certificates and internal memory.

There is no public TCP/TLS/WebSocket VTable or unified TLS config. Platform
configuration stays in Port headers; generic headers expose no SDK types.

A platform may build several backends for different instances or select only
one. Unified Ops and instance binding/ownership remain independent of build choice.
