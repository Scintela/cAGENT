# OpenVela Session Storage files

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
`CONFIG_AGENT_PORT_OPENVELA_STORAGE`. The Port target builds its own copy of
the common POSIX I/O source; do not also build the standalone POSIX adapter
target into the same firmware. Build and link the JSONL provider and bounded
JSON codec separately; selecting this Port alone does not include them. For a
regular CMake build of the library, those components use
`AGENT_BUILD_SESSION_JSONL=ON` and `AGENT_BUILD_JSON_CODEC=ON`.
