# ESP-IDF File Storage

## Shared file entry

`agent_espidf_file_store.h` binds `agent_file_store_t` to a compatible, already
mounted VFS directory, independently of JSONL and the JSON codec:

```c
#include <agent_espidf_file_store.h>

static agent_espidf_file_store_t files;
static char paths[512], user[2048];
agent_file_store_t store;
agent_string_view_t text;
agent_espidf_file_store_config_t config = {
    "/data/user", paths, sizeof(paths), true, false
};
agent_error_t status = agent_port_espidf_file_store_init(&files, &config, &store);
if (status == AGENT_OK)
    status = agent_file_read_text(&store, agent_string_view("USER.md", 7u),
                                 user, sizeof(user), sizeof(user) - 1u, &text);
```

Select `CONFIG_AGENT_FILE_STORE`, `CONFIG_AGENT_POSIX_FILE_STORE`, and
`CONFIG_AGENT_PORT_ESPIDF_FILE_STORE`. The main cagent component compiles each
shared source once and exports its headers; this Port only adds its initializer.
For JSONL, also select `CONFIG_AGENT_SESSION_JSONL` and initialize
`agent_session_jsonl_files_t` using a **separate** name buffer. No new Core
filesystem pointer is introduced. Writable stores use `read_only=false`.

ESP-IDF's [VFS syscall implementation](https://github.com/espressif/esp-idf/blob/v5.5/components/vfs/vfs.c)
provides stat and directory access, not a generic lstat binding. The component
explicitly builds the shared backend with `AGENT_POSIX_FILE_STORE_NO_SYMLINKS=1`:
only mounts without symlink support are within this profile's scope. Custom
builds of these sources must apply the same definition. Root and ancestors must
be trusted and stable; this is not a hostile-directory sandbox.

Enable VFS I/O and directory support. Validate stat/fstat, offset reads,
ftruncate, fsync, enumeration and rename on the selected filesystem. Read-only
loading requires only size/read; enumeration requires directory support.
Some VFS mounts cannot synchronize directories; leave `sync_directory=false`
unless the filesystem supports it. If true, init verifies directory fsync and
fails explicitly when unavailable. A required file fsync failure is not hidden.
Rename-over-existing and power-loss outcomes must be verified on target;
Host tests do not establish these guarantees.

Buffers are caller-owned and disjoint. No mount, format, directory creation,
retention or authorization policy is implied. Replacement requires two full
paths in scratch; `published=true` can accompany a directory sync error.
See [shared file semantics](../../posix/storage/README.md).

## Legacy Session-only entry

This optional Port binds the JSONL Session provider to the ESP-IDF VFS through
the shared POSIX file operations. It does not mount, format, select a
partition, or manage filesystem retention. The application must first mount
a filesystem at a trusted absolute path and provide all JSONL buffers.

```c
agent_espidf_session_files_t files;
agent_session_jsonl_config_t config = {0};
char path[256];

/* Populate config's JSONL buffers before calling agent_session_jsonl_init. */
agent_error_t status = agent_port_espidf_session_files_init(
    &files, "/data/sessions", path, sizeof(path), &config);
```

Keep `files`, the directory string, and `path` alive while the JSONL provider
is bound. The instance is synchronous and not thread-safe. Use a directory
dedicated to Session files. A successful VFS `fsync` call does not by itself
prove power-loss durability on every filesystem; validate `ftruncate`,
`fsync`, directory enumeration, tail repair, and restart recovery on the
chosen target and mount configuration.

Select Core `CONFIG_AGENT_SESSION_JSONL` and
`CONFIG_AGENT_SESSION_POSIX_FILES`, then `CONFIG_AGENT_PORT_ESPIDF_STORAGE`.
The Port component depends on `cagent` and `vfs`. Host contract tests check
the binding and file operations, not flash power-loss behavior.
