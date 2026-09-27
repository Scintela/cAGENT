/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* Private context registry and bounded projection. */
#pragma once

#include "core/arena_internal.h"
#include <agent/context.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agent_context_registry agent_context_registry_t;

agent_error_t agent_context_registry_init(agent_context_registry_t** registry,
                                               agent_arena_t* arena, size_t capacity);
agent_error_t agent_context_build(const agent_context_registry_t* registry,
                                       const agent_context_request_t* request,
                                       agent_arena_t* arena, size_t max_bytes,
                                       const agent_text_sink_t* output);

#ifdef __cplusplus
}
#endif
