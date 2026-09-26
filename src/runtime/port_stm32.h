/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* Private STM32 board-service boundary; RTOS and network stack remain external. */
#pragma once

#include <agent/error.h>
#include <agent/runtime.h>

#ifdef __cplusplus
extern "C" {
#endif

agent_error_t agent_port_stm32_init(agent_runtime_t* runtime,
                                         const agent_runtime_t* services);

#ifdef __cplusplus
}
#endif
