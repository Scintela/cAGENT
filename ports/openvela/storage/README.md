# OpenVela File Storage

## Shared file entry

`agent_openvela_file_store.h` exposes byte-file ops over an application-mounted
trusted directory, independently of JSONL:

```c
#include <agent_openvela_file_store.h>

static agent_openvela_file_store_t files;
static char paths[512], user[2048];
agent_file_store_t store;
agent_string_view_t text;
agent_openvela_file_store_config_t config = {
    "/data/user", paths, sizeof(paths), false, false
};
agent_error_t status = agent_port_openvela_file_store_init(&files, &config, &store);
if (status == AGENT_OK)
    status = agent_file_read_text(&store, agent_string_view("USER.md", 7u),
                                 user, sizeof(user), sizeof(user) - 1u, &text);
```

Select `CONFIG_AGENT_FILE_STORE` and `CONFIG_AGENT_PORT_OPENVELA_FILE_STORE`.
The NuttX CMake Port builds missing common targets or reuses already-defined
`cagent_file_store` / `cagent_posix_file_store` targets, compiling shared sources
only once. Link `cagent_openvela_file_store`. Kconfig is optional when manually
including the common, POSIX and platform sources in an application build.

For JSONL, build the JSON codec, JSONL Provider and `cagent_session_jsonl_files`
bridge as separate packages; use `agent_session_jsonl_files_init` with independent
name scratch. In regular CMake assembly, enable `AGENT_BUILD_FILE_STORE`,
`AGENT_BUILD_JSON_CODEC` and `AGENT_BUILD_SESSION_JSONL` before adding this Port.
Set corresponding Core build-profile values consistently across all sources.

The shared backend requires compatible lstat/fstat, read/write/lseek, truncate,
fsync and directory calls; replacement also needs exclusive create and rename.
Leaf symlinks/nonregular files are refused. Root and ancestors must nevertheless
remain trusted and stable; no protection against hostile concurrent replacement
is claimed. Read-only stores omit write callbacks. `sync_directory=true` verifies
directory fsync at init; unsupported filesystems fail, rather than claiming a
stronger durability guarantee. Validate actual filesystem and restart/power-loss
behavior on target. Current RISC-V/NuttX checks compile sources, not a full firmware.

See [shared file semantics](../../posix/storage/README.md) for borrowed
buffers, two-path replacement scratch and `published` outcomes. The application
still owns mounting, directories, permissions, retention and flash wear.

## Legacy Session-only entry

This optional Port binds the JSONL Session provider to an application-mounted
OpenVela/NuttX filesystem. Its `storage.c` owns the platform-facing binding;
the existing POSIX file adapter supplies bounded path encoding and the eight
JSONL file operations. No filesystem is mounted or formatted by this package.

```c
agent_openvela_session_files_t files;
agent_session_jsonl_config_t config = {0};
char path[256];

/* Populate config's JSONL buffers before calling agent_session_jsonl_init. */
agent_error_t status = agent_port_openvela_session_files_init(
    &files, "/data/sessions", path, sizeof(path), &config);
```

Keep the state, directory string, and path buffer alive while JSONL is bound.
The application must serialize access, own a trusted dedicated directory,
and choose a filesystem supporting `open/read/write/lseek/ftruncate/fsync`,
`stat`, and directory enumeration. It also owns retention, flash wear and
mount/unmount policy. Validate tail repair, `fsync`, and restart recovery on
the actual board and mounted filesystem; Host tests do not prove power-loss
durability.

Include `ports/openvela/Kconfig` and `ports/openvela` in the application's
NuttX build, select Core `CONFIG_AGENT_SESSION_JSONL`, then select
`CONFIG_AGENT_PORT_OPENVELA_STORAGE`. The Port target reuses a prebuilt POSIX
Session target, or builds it once when missing. Build and link the JSONL provider and bounded
JSON codec separately; selecting this Port alone does not include them. For a
regular CMake build of the library, those components use
`AGENT_BUILD_SESSION_JSONL=ON` and `AGENT_BUILD_JSON_CODEC=ON`.
