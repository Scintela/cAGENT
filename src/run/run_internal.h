/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* Private turn state, cancellation, and ReAct dispatch. */
#pragma once

#include "types_internal.h"
#include <agent/run.h>
#include <agent/runtime.h>

#ifdef __cplusplus
extern "C" {
#endif

struct agent_cancel_token {
    agent_sync_t sync;
    bool requested;
};

agent_error_t agent_turn_workspace_init(agent_turn_t** turn, agent_t* owner,
                                             agent_arena_t* arena);
agent_error_t agent_react_step(agent_turn_t* turn, agent_step_result_t* result);
void agent_cancel_token_init(agent_cancel_token_t* token, const agent_sync_t* sync);

/* Caller holds the active-turn synchronization region. */
void agent_cancel_token_request_locked(agent_cancel_token_t* token);

#ifdef __cplusplus
}
#endif
