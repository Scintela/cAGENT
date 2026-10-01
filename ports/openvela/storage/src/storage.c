/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* OpenVela/NuttX Session file binding; the application owns the mount. */
#include <agent_openvela_session_files.h>

agent_error_t agent_port_openvela_session_files_init(
    agent_openvela_session_files_t* state, const char* mounted_directory,
    char* path_buffer, size_t path_capacity,
    agent_session_jsonl_config_t* config)
{
    agent_posix_session_files_t files;
    agent_error_t status;

    if (!state || !config) return AGENT_ERROR_INVALID;
    status = agent_posix_session_files_init(&files, mounted_directory,
                                             path_buffer, path_capacity);
    if (status != AGENT_OK) return status;
    state->files = files;
    config->files = agent_posix_session_file_ops();
    config->file_context = &state->files;
    return AGENT_OK;
}
