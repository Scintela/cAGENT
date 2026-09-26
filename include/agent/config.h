/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* Core workspace capacities and initialization configuration. Interface draft; declarations do not imply an implemented feature. */
#pragma once

#include <agent/runtime.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Physical capacities, immutable after init; zero disables optional slots. */
typedef struct {
    size_t max_tools;              /* Tool slots; 0 disables registration. */
    size_t max_contexts;           /* Context provider slots; 0 disables registration. */
    size_t max_skills;             /* Skill slots; 0 disables registration. */
    size_t max_sessions;           /* Session identity slots; at least 1. */
    size_t session_event_capacity; /* Global bounded event descriptors. */
    size_t session_payload_bytes;  /* Global session content/identity pool. */
    size_t scratch_bytes;          /* Reusable context/model/tool/turn storage. */
    size_t max_input_bytes;        /* Per-turn user input ceiling. */
    size_t max_context_bytes;      /* Combined system/context/skill byte ceiling. */
    size_t max_schema_bytes;       /* Per-tool JSON Schema byte ceiling. */
    size_t max_arguments_bytes;    /* Per-call argument object byte ceiling. */
    size_t max_tool_output_bytes;  /* Per-handler result byte ceiling. */
    size_t max_model_output_bytes; /* Per-model-call text byte ceiling. */
    size_t max_model_tool_calls;   /* Calls retained from a single model response. */
    size_t max_name_bytes;         /* Tool/context/skill name byte ceiling. */
    size_t max_description_bytes;  /* Per-contribution description byte ceiling. */
    size_t max_identifier_bytes;   /* Session/trace/call identifier byte ceiling. */
    size_t max_json_depth;         /* Codec nesting ceiling; nonzero. */
} agent_resource_config_t;

/* Initialization configuration; value copied, referenced objects borrowed. */
typedef struct {
    agent_string_view_t system_prompt; /* Optional immutable instructions. */
    agent_resource_config_t resources; /* Core storage capacities. */
    agent_limits_t limits;             /* Default execution limits. */
    agent_runtime_t runtime;           /* Supplied platform services, including clock. */
} agent_config_t;

/* Return a bounded general-purpose configuration. */
agent_config_t agent_config_default(void);

/* Return reduced storage capacities and limits, not just shorter timeouts. */
agent_config_t agent_config_tiny(void);

#ifdef __cplusplus
}
#endif
