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

typedef struct agent_session_turn agent_session_turn_t;

/* Current-turn facts and source text live in the caller's turn scratch. */
agent_error_t agent_session_turn_open(agent_session_turn_t** transaction,
                                      agent_arena_t* arena,
                                      const agent_session_storage_t* storage,
                                      agent_string_view_t session_id,
                                      agent_string_view_t input);
agent_error_t agent_session_append(agent_session_turn_t* transaction,
                                   const agent_message_view_t* message);
agent_error_t agent_session_turn_finish(agent_session_turn_t* transaction,
                                        agent_session_turn_outcome_t outcome);
agent_error_t agent_session_project(const agent_session_turn_t* transaction,
                                    agent_arena_t* arena, uint32_t max_history_turns,
                                    const agent_message_view_t** messages, size_t* count);

#ifdef __cplusplus
}
#endif
