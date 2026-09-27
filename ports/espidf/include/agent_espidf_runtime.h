/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* ESP-IDF Runtime builder. */
#pragma once

#include <agent/error.h>
#include <agent/runtime.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Populate a minimal Runtime using the ESP-IDF monotonic microsecond timer. */
agent_error_t agent_port_espidf_runtime_init(agent_runtime_t* out);

#ifdef __cplusplus
}
#endif
