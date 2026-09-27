/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* ESP-IDF Runtime clock adapter. */

#include <agent_espidf_runtime.h>

#include <esp_timer.h>

static uint64_t agent_port_espidf_now_ms(void* context)
{
    (void)context;
    return (uint64_t)esp_timer_get_time() / 1000u;
}

agent_error_t agent_port_espidf_runtime_init(agent_runtime_t* out)
{
    if (out == NULL)
    {
        return AGENT_ERROR_INVALID;
    }

    *out = (agent_runtime_t){0};
    out->now_ms = agent_port_espidf_now_ms;
    return AGENT_OK;
}
