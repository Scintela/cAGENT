/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* Private tool registry, schema validation, and guarded dispatch. */
#pragma once

#include "types_internal.h"
#include <agent/config.h>
#include <agent/model.h>
#include <agent/tool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agent_tool_registry agent_tool_registry_t;

agent_error_t agent_tool_registry_init(agent_tool_registry_t** registry,
                                            agent_arena_t* arena,
                                            const agent_resource_config_t* resources);
const agent_tool_t* agent_tool_registry_find(const agent_tool_registry_t* registry,
                                             agent_string_view_t name);
agent_error_t agent_tool_registry_project(const agent_tool_registry_t* registry,
                                               agent_tool_view_t* views, size_t capacity,
                                               size_t* count);
agent_error_t agent_tool_schema_validate(agent_string_view_t schema,
                                              const agent_resource_config_t* resources);
agent_error_t agent_tool_guard_check(const agent_tool_t* tool,
                                          const agent_tool_context_t* context,
                                          const agent_resource_config_t* resources);
agent_error_t agent_tool_dispatch(const agent_tool_t* tool,
                                       const agent_tool_context_t* context,
                                       const agent_text_sink_t* output);

#ifdef __cplusplus
}
#endif
