/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 tanglinjie */
#include <agent_rtthread_file_store.h>
agent_error_t agent_port_rtthread_file_store_init(agent_rtthread_file_store_t* state,
    const agent_rtthread_file_store_config_t* config, agent_file_store_t* store)
{
    return agent_posix_file_store_init(state, config, store);
}
