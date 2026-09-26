/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* Bounded in-memory conversations, not durable storage. Interface draft; declarations do not imply an implemented feature. */
#pragma once

#include <agent/error.h>
#include <agent/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Clear an existing conversation while retaining its identity/slot. Returns: AGENT_OK; NOT_FOUND, INVALID or BUSY. Does not create a missing session or erase external storage. */
agent_error_t agent_session_clear(agent_t* agent, agent_string_view_t session_id);

/* Clear all existing conversations without releasing identity slots. Returns: AGENT_OK; INVALID or BUSY. An empty manager is a successful no-op. */
agent_error_t agent_session_clear_all(agent_t* agent);

/* Remove one identity and its history, releasing its slot. */
agent_error_t agent_session_remove(agent_t* agent, agent_string_view_t session_id);

/* Count allocated identities, including empty retained conversations. Returns: AGENT_OK or INVALID. */
agent_error_t agent_session_count(const agent_t* agent, size_t* count);

#ifdef __cplusplus
}
#endif
