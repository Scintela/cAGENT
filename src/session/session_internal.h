/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* Private bounded conversation transactions and model projection. */
#pragma once

#include "core/arena_internal.h"
#include <agent/config.h>
#include <agent/model.h>
#include <agent/session.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agent_session_manager agent_session_manager_t;
typedef struct agent_session_turn agent_session_turn_t;

/* Legacy RAM manager; replace with Storage Provider binding before implementation. */
agent_error_t agent_session_manager_init(agent_session_manager_t** manager,
                                         agent_arena_t* arena);
agent_error_t agent_session_turn_open(agent_session_manager_t* manager,
                                           agent_string_view_t session_id,
                                           agent_string_view_t input,
                                           agent_session_turn_t** transaction);
agent_error_t agent_session_reserve_tool_result(agent_session_turn_t* transaction,
                                                     agent_string_view_t call_id,
                                                     size_t max_result_bytes);
agent_error_t agent_session_append(agent_session_turn_t* transaction,
                                        const agent_message_view_t* message);
void agent_session_turn_finish(agent_session_turn_t* transaction, agent_error_t status,
                               const agent_run_summary_t* summary);
agent_error_t agent_session_project(const agent_session_turn_t* transaction,
                                         agent_arena_t* arena,
                                         const agent_message_view_t** messages, size_t* count);

#ifdef __cplusplus
}
#endif
