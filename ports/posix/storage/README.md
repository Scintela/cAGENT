# POSIX JSONL Session files

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
