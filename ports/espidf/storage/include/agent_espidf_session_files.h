/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* ESP-IDF VFS binding for the JSONL Session file provider. */
#pragma once

#include <agent_posix_session_files.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Borrowed VFS directory and path scratch; one active Session at a time. */
typedef struct {
    agent_posix_session_files_t files;
} agent_espidf_session_files_t;

/* Bind file ops into an otherwise caller-configured JSONL provider. */
agent_error_t agent_port_espidf_session_files_init(
    agent_espidf_session_files_t* state, const char* mounted_directory,
    char* path_buffer, size_t path_capacity,
    agent_session_jsonl_config_t* config);

#ifdef __cplusplus
}
#endif
