/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* ESP-IDF VFS binding; mounting and filesystem policy belong to the application. */
#pragma once
#include <agent_posix_file_store.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef agent_posix_file_store_t agent_espidf_file_store_t;
typedef agent_posix_file_store_config_t agent_espidf_file_store_config_t;

/* Caller-owned root and scratch; synchronous, serialized, no implicit mount. */
agent_error_t agent_port_espidf_file_store_init(agent_espidf_file_store_t* state,
    const agent_espidf_file_store_config_t* config, agent_file_store_t* store);

#ifdef __cplusplus
}
#endif
