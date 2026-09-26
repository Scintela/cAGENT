/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* Private ordered skill registry and bounded projection. */
#pragma once

#include "types_internal.h"
#include <agent/skill.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agent_skill_registry agent_skill_registry_t;

agent_error_t agent_skill_registry_init(agent_skill_registry_t** registry,
                                             agent_arena_t* arena, size_t capacity);
agent_error_t agent_skill_project(const agent_skill_registry_t* registry,
                                       size_t max_bytes, const agent_text_sink_t* output);

#ifdef __cplusplus
}
#endif
