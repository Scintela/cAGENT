# Host / POSIX

Start with the [quickstart](../getting-started/quickstart.md). Host uses CMake
and application Runtime/Model/Transport, without Kconfig.

## File Storage

Enable `AGENT_BUILD_FILE_STORE=ON` and `AGENT_BUILD_POSIX_FILE_STORE=ON`;
link `cagent::posix_file_store`.

```c
#include <agent_posix_file_store.h>

static agent_posix_file_store_t state;
static char path[512];
static agent_file_store_t store;

agent_error_t open_documents(const char* trusted_directory)
{
    const agent_posix_file_store_config_t config = {
        .directory = trusted_directory,
        .path_buffer = path,
        .path_capacity = sizeof(path),
        .read_only = false,
        .sync_directory = true
    };
    return agent_posix_file_store_init(&state, &config, &store);
}
```

Supply an existing absolute trusted root; the directory string must stay alive.
Store retains no long-lived descriptor. Serialize shared state/path buffers.
Initialization probes directory fsync; handle failure rather than assuming every
mount supports it.

JSONL binds the byte contract through its domain bridge. Explicitly isolate
Memory user/notes roots. See [Storage](../api/storage.md) for publication/failure
semantics.

## Networking

No official Host Runtime builder or libcurl adapter is implemented. Applications
may implement synchronous Transport Ops with the header/body/budget/cancel/TLS
contract. Fake HTTP in tests is not a production backend.
