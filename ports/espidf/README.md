# ESP-IDF Port

This optional package will own ESP-IDF headers, component metadata, Runtime clock/log glue, and
an `esp_http_client` Transport adapter. The adapter must use caller-controlled bounds and obey the
synchronous `agent_transport_ops_t` callback contract.

No ESP-IDF implementation is shipped yet.
