/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* OpenVela/NuttX filesystem binding for JSONL Session storage. */
#pragma once

#include <agent_posix_session_files.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Borrowed mounted directory and path scratch; serialize use of one instance. */
typedef struct {
    agent_posix_session_files_t files;
} agent_openvela_session_files_t;

/* Bind file ops into an otherwise caller-configured JSONL provider. */
agent_error_t agent_port_openvela_session_files_init(
    agent_openvela_session_files_t* state, const char* mounted_directory,
    char* path_buffer, size_t path_capacity,
    agent_session_jsonl_config_t* config);

#ifdef __cplusplus
}
#endif
