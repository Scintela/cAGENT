# POSIX File Storage

## Shared byte-file backend

`agent_posix_file_store.h` is the preferred entry for shared file access, with
no JSONL dependency. It implements `agent_file_store_t` over one existing,
trusted absolute directory. It does not mount, format or create directories.

```c
#include <agent_posix_file_store.h>

static agent_posix_file_store_t files;
static char paths[512];
static char user[2048];
agent_file_store_t store;
agent_string_view_t text;
agent_posix_file_store_config_t config = {
    "/data/user", paths, sizeof(paths), false, false
};
agent_error_t status = agent_posix_file_store_init(&files, &config, &store);
if (status == AGENT_OK)
    status = agent_file_read_text(&store, agent_string_view("USER.md", 7u),
                                 user, sizeof(user), sizeof(user) - 1u, &text);
```

The config is copied; state, root string and caller scratch remain valid. Each
operation closes its descriptors before return, including failure paths. Access
is synchronous and serialized by the caller. Leaf symlinks/nonregular files
are rejected by the standard profile; enumeration skips them and private temporary files. This is **not
a sandbox against concurrent untrusted directory mutation**: the whole root and
its ancestors must be trusted and stable. Use a different backend if that
assumption cannot hold. Inputs/outputs must not overlap path scratch or state.
`AGENT_POSIX_FILE_STORE_NO_SYMLINKS=1` is an explicit build profile for VFS mounts
without symlink support or lstat (used by ESP-IDF). It uses stat, does not provide
symlink rejection, and must never be used on a filesystem that admits symlinks.

`read_only=true` binds only size/read/visit. For ordinary paths, scratch needs
`root + slash + name + NUL`. Replacement requires **two full paths** at once:
target plus private `.cagent-xxxxxxxx` temporary path. Too-small scratch returns
`AGENT_ERROR_CAPACITY` before modifying the target.

Replacement writes a same-directory exclusive temporary file, synchronizes it,
closes it, then renames it over the target. New files have mode 0600; replacement
does not preserve old permissions or metadata. It never truncates the old target
first. On failure before rename, cleanup is attempted and `published=false`;
after rename, `published=true`, even if directory synchronization fails. Do not
blindly retry an ambiguous result. Stale private files after a crash are hidden
from enumeration, not automatically deleted; applications own maintenance.

`sync_directory=true` verifies directory `fsync` during init and requests it
after sync/remove/replace. Unsupported operations fail explicitly. With `false`,
file `fsync` is still requested, but directory metadata durability is not claimed.
Both modes require filesystem-specific reboot/power-loss validation. POSIX
rename publication is not itself proof of crash consistency on an embedded VFS.

Host build: `AGENT_BUILD_FILE_STORE=ON` and `AGENT_BUILD_POSIX_FILE_STORE=ON`,
then link `cagent::posix_file_store`. Core-only and RAM Session builds do not
require these components. See [file contracts](../../../providers/storage/files/README.md).

## Legacy Session-only entry

This optional adapter implements `agent_session_jsonl_file_ops_t` for an
application-owned, already mounted directory. The JSONL provider still owns
record encoding and tail recovery; Core remains filesystem-independent.

```c
agent_posix_session_files_t files;
char path[256];
agent_session_jsonl_config_t config = {0};

/* The application provides an existing directory and all JSONL buffers. */
agent_posix_session_files_init(&files, "/data/sessions", path, sizeof(path));
config.files = agent_posix_session_file_ops();
config.file_context = &files;
```

The directory must be an existing absolute path. Its string, `files`, and
`path` remain alive while JSONL is bound.
One instance is synchronous and not thread-safe. The application mounts and
owns the filesystem, chooses a private directory, and handles capacity,
retention, encryption, and unmounting. Session IDs are hex-encoded as
`session-<hex>.jsonl`; `count` and `clear_all` act only on matching names.
The directory must not be writable by an untrusted party: encoding names does
not defend against symlink substitution or concurrent replacement.

The adapter needs working `open`, `read`, `write`, `lseek`, `ftruncate`,
`fsync`, `stat`, `opendir`, `readdir`, and `unlink`. It returns errors rather
than pretending an unsupported `fsync` succeeded. A successful `fsync` call
is not, by itself, a verified power-loss guarantee for every filesystem.
`clear_all` can partially remove files before reporting an I/O error.

On Host, build with `AGENT_BUILD_JSON_CODEC=ON`,
`AGENT_BUILD_SESSION_JSONL=ON`, and `AGENT_BUILD_POSIX_SESSION_FILES=ON`,
then link `cagent::posix_session_files`. ESP-IDF may select
`CONFIG_AGENT_SESSION_POSIX_FILES` with `CONFIG_AGENT_SESSION_JSONL`.
NuttX/RT-Thread applications can compile this source if their configured
filesystem supplies the required calls. Hardware durability and filesystem
compatibility have not yet been validated by the Host contract tests.
