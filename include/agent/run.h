/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* Single-driver synchronous turn stepping and cooperative cancellation. */
#pragma once

#include <agent/error.h>
#include <agent/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Run phase; WAITING_MODEL/TOOL may execute blocking callbacks. */
typedef enum {
    AGENT_RUN_IDLE = 0,             /* Not started. */
    AGENT_RUN_BUILDING_CONTEXT,     /* Building bounded context. */
    AGENT_RUN_WAITING_MODEL,        /* Calling or preparing the model. */
    AGENT_RUN_WAITING_TOOL,         /* Preparing or executing a tool. */
    AGENT_RUN_WAITING_CONFIRMATION, /* Paused for explicit authorization. */
    AGENT_RUN_COMPLETED,            /* Terminal successful final. */
    AGENT_RUN_FAILED,               /* Terminal execution error. */
    AGENT_RUN_CANCELLED             /* Terminal cancellation. */
} agent_run_state_t;

/* Result of a successful step API invocation. */
typedef enum {
    AGENT_STEP_CONTINUE = 0, /* Call step again. */
    AGENT_STEP_NEED_CONFIRM, /* Submit a matching resume decision. */
    AGENT_STEP_DONE,         /* Successful terminal state. */
    AGENT_STEP_FAILED        /* Terminal error/cancellation; inspect status. */
} agent_step_kind_t;

/* Step result. Temporary views expire on the next step, resume, or end. */
typedef struct {
    agent_step_kind_t kind;              /* Progress/confirmation/terminal category. */
    agent_run_state_t state;             /* Current execution phase. */
    agent_error_t status;           /* Execution status, not the step API's return code. */
    agent_string_view_t message;         /* Optional diagnostic, never assistant final. */
    agent_tool_call_view_t pending_call; /* NEED_CONFIRM only. */
    uint64_t confirmation_id;            /* Nonzero Agent-wide monotonically issued nonce. */
    agent_string_view_t final_output;    /* Successful final; may be empty. */
    agent_run_summary_t summary;         /* Facts accumulated so far. */
} agent_step_result_t;

/* Authorization decision; a zero-initialized value never allows. */
typedef enum {
    AGENT_RESUME_INVALID = 0, /* Reject as invalid input. */
    AGENT_RESUME_DENY,        /* Do not execute the pending handler. */
    AGENT_RESUME_ALLOW        /* Authorize this one pending call. */
} agent_resume_decision_t;

/* One-shot confirmation; both nonce and call ID must match. */
typedef struct {
    agent_resume_decision_t decision; /* Explicit allow or deny. */
    uint64_t confirmation_id;         /* Copy from NEED_CONFIRM; no zero wildcard. */
    agent_string_view_t tool_call_id; /* Required exact pending ID; call-only borrow. */
} agent_resume_t;

/* Polls a turn token. Not ISR-safe; cross-task use needs runtime synchronization. */
bool agent_cancel_token_is_set(const agent_cancel_token_t* token);

/* Begins one turn on a READY Agent. Request text/user_data remain borrowed until end. */
agent_error_t agent_turn_begin(agent_t* agent, const agent_request_t* request,
                                    agent_turn_t** turn);

/* Advances one stage and may block in context, model, or tool callbacks. */
agent_error_t agent_turn_step(agent_turn_t* turn, agent_step_result_t* result);

/* Consumes a matching confirmation decision; execution occurs in a later step. */
agent_error_t agent_turn_resume(agent_turn_t* turn, const agent_resume_t* resume);

/* Ends a turn, invalidates its views, and returns the Agent to READY. */
void agent_turn_end(agent_turn_t* turn);

/* Requests cooperative cancellation. It is not ISR-safe and does not end a turn. */
agent_error_t agent_cancel(agent_t* agent);

#ifdef __cplusplus
}
#endif
