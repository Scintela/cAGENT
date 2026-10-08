# ESP-IDF Integration

`ports/espidf/` contains runtime, transport and storage packages. Core includes
no ESP-IDF SDK headers; applications include each package's `agent_espidf_*.h`.

## Build

Add the repository as an ESP-IDF component named `cagent`, and add
`ports/espidf` to component search paths. Port REQUIRES refers to cagent; update
dependencies consistently if renaming it.

| Kconfig switch | Purpose |
|---|---|
| CONFIG_AGENT_PROVIDER_OPENAI | Build OpenAI/private JSON in the Core component |
| CONFIG_AGENT_PORT_ESPIDF_RUNTIME | esp_timer Runtime |
| CONFIG_AGENT_PORT_ESPIDF_TRANSPORT | esp_http_client adapter |
| CONFIG_AGENT_FILE_STORE / CONFIG_AGENT_POSIX_FILE_STORE | Shared byte contract/POSIX backend |
| CONFIG_AGENT_PORT_ESPIDF_FILE_STORE | ESP-IDF file wrapper |
| CONFIG_AGENT_SESSION_JSONL / CONFIG_AGENT_MEMORY_MARKDOWN | Independent domain backends |

Runtime/Transport declare SDK dependencies when enabled. The file wrapper needs
both File Store and POSIX enabled. Enabling it does not bind JSONL/Markdown.

## Runtime and Transport

`agent_port_espidf_runtime_init(&runtime)` installs the esp_timer monotonic clock.
The minimal builder does not install heap allocation or cross-task cancellation
locks; add optional services according to product requirements.

`agent_port_espidf_transport_init(&binding, &state, &config)` accepts Runtime,
request_timeout_ms, URL/header buffers and response-header arrays. HTTPS uses
borrowed cert_pem or use_crt_bundle; bundles need SDK bundle configuration.
Reject conflicting/untrusted configuration rather than disabling verification.

Model Provider receives `agent_transport_t`, not esp_http_client_handle_t.
HTTP/TLS SDK heap is outside Core Workspace; measure its peak and task stack.

## Filesystems

Mount VFS and create a trusted root before `agent_port_espidf_file_store_init()`.
Configuration matches POSIX. The current no-symlink profile is appropriate only
for trusted mounts that genuinely do not support symlinks.

Missing truncate/rename/directory-sync support limits JSONL/replacement guarantees.
Mounts, formatting, partitions, space management and power-loss tests belong to
the application/BSP.

## Verification

```sh
bash tests/ports/espidf/compile.sh
bash tests/build/file_store/compile.sh
```

These are fake-SDK Host/build tests, not acceptance of a real SDK release,
networking, certificates or filesystem durability.
