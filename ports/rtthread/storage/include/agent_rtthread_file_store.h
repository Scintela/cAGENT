/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 tanglinjie */
#pragma once
#include <agent_posix_file_store.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef agent_posix_file_store_t agent_rtthread_file_store_t;
typedef agent_posix_file_store_config_t agent_rtthread_file_store_config_t;

/* Bind an already mounted DFS/POSIX directory; filesystem capabilities remain BSP requirements. */
agent_error_t agent_port_rtthread_file_store_init(agent_rtthread_file_store_t* state,
    const agent_rtthread_file_store_config_t* config, agent_file_store_t* store);
#ifdef __cplusplus
}
#endif
