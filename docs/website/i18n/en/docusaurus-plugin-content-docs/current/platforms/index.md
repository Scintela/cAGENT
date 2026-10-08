# Platform Integration

Core keeps the same application contracts; Ports provide platform services.
Build selection chooses implementations and initialization binds instances. The
library never automatically connects networking, mounts/formats filesystems or
selects trust roots.

| Platform | Runtime | HTTP/TLS | File storage | Guide |
|---|---|---|---|---|
| Host/POSIX | Application-supplied | Application-supplied, no official curl backend | POSIX implemented | [Host](host.md) |
| ESP-IDF | Minimal esp_timer clock | esp_http_client, CA/bundle | VFS/POSIX wrapper | [ESP-IDF](espidf.md) |
| OpenVela/NuttX | Monotonic clock | webclient; application-authenticated TLS | NuttX/POSIX wrapper | [OpenVela](openvela.md) |
| RT-Thread | 5.1+ tick extension and cancel sync | WebClient 2.3, nonempty POST; application-authenticated TLS | DFS/POSIX wrapper | [RT-Thread](rtthread.md) |

## Common Assembly Order

1. Initialize application networking and mounted filesystems.
2. Initialize Runtime with a dependable monotonic clock.
3. Initialize Transport/File Store and keep their state/buffers alive.
4. Initialize Model, Storage and Memory Providers.
5. Initialize Agent, borrow bindings/register sources, start, then run on the worker.
6. End all active calls before destroying Agent, wrapper and platform services.

## Validation Levels

Host tests cover simulated SDKs, header compilation, fault injection and build
matrices. They do not certify arbitrary SDK/BSP/filesystem/TLS combinations.
RT-Thread also has real-header checks, still not firmware/network acceptance.

Implement only needed [Runtime](../api/runtime.md), [Transport](../api/transport.md)
or [File Store](../api/storage.md) capabilities. Do not add large TLS/thread/file
abstractions merely for directory symmetry.
