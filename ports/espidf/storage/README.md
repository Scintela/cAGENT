# ESP-IDF Session Storage files

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
