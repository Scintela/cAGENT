/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* Optional POSIX file operations for the JSONL Session provider. */
#pragma once

#include <agent_session_jsonl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Borrowed directory and path scratch; serialize access to one instance. */
typedef struct {
    const char* directory;
    size_t directory_length;
    char* path;
    size_t path_capacity;
} agent_posix_session_files_t;

/* The application mounts and owns an existing, trusted directory. */
agent_error_t agent_posix_session_files_init(agent_posix_session_files_t* files,
                                              const char* directory,
                                              char* path_buffer, size_t path_capacity);

/* Set config.files to this table and config.file_context to the initialized state. */
agent_session_jsonl_file_ops_t agent_posix_session_file_ops(void);

#ifdef __cplusplus
}
#endif
