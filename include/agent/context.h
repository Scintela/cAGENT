/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* Optional, bounded dynamic context contributions. Interface draft; declarations do not imply an implemented feature. */
#pragma once

#include <agent/error.h>
#include <agent/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Inputs borrowed only during a context build callback.  */
typedef struct {
    const agent_request_t* request;     /* Borrowed request with copied effective limits. */
    const agent_cancel_token_t* cancel; /* Borrowed turn cancellation token. */
    uint64_t deadline_ms;               /* Absolute overall deadline, 0=none. */
} agent_context_request_t;

/* Produce context synchronously; Core copies each sink chunk. */
typedef agent_error_t (*agent_context_build_fn)(void* user_data,
                                                      const agent_context_request_t* request,
                                                      const agent_text_sink_t* output);

/* Context definition; value copied, name/state borrowed until removal.  */
typedef struct {
    agent_string_view_t name;     /* Unique, nonempty, immutable name. */
    int32_t priority;             /* Higher values first; ties follow registration order. */
    bool required;                /* Failure/overflow aborts the turn when true. */
    agent_context_build_fn build; /* Required callback. */
    void* user_data;              /* Borrowed application state. */
} agent_context_provider_t;

/* Register a context provider in CONFIGURING/READY. */
agent_error_t agent_register_context(agent_t* agent,
                                          const agent_context_provider_t* provider);

/* Remove a provider without destroying its external state. Returns: AGENT_OK; INVALID, NOT_FOUND or BUSY. */
agent_error_t agent_unregister_context(agent_t* agent, agent_string_view_t name);

#ifdef __cplusplus
}
#endif
