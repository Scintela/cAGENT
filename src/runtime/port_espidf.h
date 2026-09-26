/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* Private ESP-IDF Runtime assembly; it does not start I/O or tasks. */
#pragma once

#include <agent/error.h>
#include <agent/runtime.h>

#ifdef __cplusplus
extern "C" {
#endif

agent_error_t agent_port_espidf_init(agent_runtime_t* runtime);

#ifdef __cplusplus
}
#endif
