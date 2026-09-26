/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* Common handles, bounded views and cross-module execution values. Interface draft; declarations do not imply an implemented feature. */
#pragma once

#include <agent/error.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque Agent; one driver task per instance.  */
typedef struct agent agent_t;
/* Opaque turn, borrowed from its Agent workspace.  */
typedef struct agent_turn agent_turn_t;
/* Cancellation token, valid only during its owning turn.  */
typedef struct agent_cancel_token agent_cancel_token_t;

/* Length-delimited text; no implicit NUL terminator. */
typedef struct {
    const char* data; /* BORROWED bytes. */
    size_t size;      /* Byte count, excluding any terminator. */
} agent_string_view_t;

/* String-literal aggregate initializer; use agent_string_view() in expressions. */
#define AGENT_SV_LITERAL(literal) {(literal), sizeof(literal) - 1u}

/* Construct a borrowed view without allocation or validation. Returns: View with the supplied pointer and length. */
static inline agent_string_view_t agent_string_view(const char* data, size_t size)
{
    agent_string_view_t view;
    view.data = data;
    view.size = size;
    return view;
}

/* Test the length of a valid view. Returns: true when the length is zero; does not validate the pointer. */
static inline bool agent_string_view_is_empty(agent_string_view_t view)
{
    return view.size == 0u;
}

/* Workspace requirement; excludes external provider/RTOS/TLS memory.  */
typedef struct {
    size_t size;      /* Required byte count for an aligned base address. */
    size_t alignment; /* Required nonzero base-address alignment in bytes. */
} agent_memory_plan_t;

/* Bounded text sink; callbacks and context are BORROWED for the call. */
typedef struct {
    agent_error_t (*write)(void* context, agent_string_view_t text); /* Required writer. */
    void* context; /* Writer-owned state; never freed by the producer. */
} agent_text_sink_t;

/* Tool call; all views last only for the consuming callback.  */
typedef struct {
    agent_string_view_t id;             /* Nonempty correlation identifier. */
    agent_string_view_t name;           /* Registered tool name. */
    agent_string_view_t arguments_json; /* Complete JSON object, not escaped JSON text. */
} agent_tool_call_view_t;

/* Per-turn limits; request overrides replace the entire value. */
typedef struct {
    uint32_t max_steps;            /* Maximum model iterations; must be nonzero. */
    uint32_t timeout_ms;           /* Overall deadline, including confirmation wait; 0=none. */
    uint32_t per_model_timeout_ms; /* Model call timeout; 0=overall deadline only. */
    uint32_t per_tool_timeout_ms;  /* Tool call timeout; 0=overall deadline only. */
    uint32_t max_tool_calls;       /* Maximum attempted handler calls; 0=disable tools. */
    uint32_t max_output_tokens;    /* Per-model-call requested budget; 0=unspecified. */
} agent_limits_t;

/* Default limits, matching agent_config_default().  */
#define AGENT_LIMITS_DEFAULT {8u, 30000u, 15000u, 3000u, 4u, 512u}

/* Per-turn facts; failure/cancellation never implies no device effects.  */
typedef struct {
    uint32_t model_calls;      /* Number of provider complete invocations. */
    uint32_t tool_calls;       /* Number of handler invocations. */
    uint32_t tool_succeeded;   /* Handler calls returning AGENT_OK. */
    uint32_t tool_failed;      /* Handler calls returning an error. */
    uint32_t tool_denied;      /* Calls rejected before invoking a handler. */
    bool final_valid;          /* A complete assistant final is available, possibly empty. */
    bool tools_executed;       /* At least one handler was invoked, effects may be unknown. */
    uint64_t elapsed_ms;       /* Turn duration including confirmation wait. */
    size_t scratch_peak_bytes; /* Core scratch high-water mark for this turn. */
} agent_run_summary_t;

/* Agent cumulative snapshot; counters saturate rather than wrap.  */
typedef struct {
    uint32_t runs;                      /* Turns successfully begun. */
    uint32_t completed_runs;            /* Successful terminal turns. */
    uint32_t failed_runs;               /* Failed terminal turns, excluding cancellation. */
    uint32_t cancelled_runs;            /* Cancelled turns, including unfinished end. */
    uint32_t timeout_runs;              /* Subset of failed_runs ending on timeout. */
    uint32_t iterations;                /* Total model iterations. */
    uint32_t model_calls;               /* Total provider calls. */
    uint32_t tool_calls;                /* Total attempted handler calls. */
    uint64_t last_run_elapsed_ms;       /* Last terminal turn duration. */
    size_t last_run_scratch_peak_bytes; /* Last terminal turn high-water mark. */
    size_t scratch_peak_bytes;          /* Lifetime scratch high-water mark. */
} agent_stats_t;

/* Turn request; descriptor and effective limits are copied at begin. */
typedef struct {
    agent_string_view_t session_id; /* Empty selects the default session. */
    agent_string_view_t input;      /* Nonempty user input, immutable during the turn. */
    agent_string_view_t trace_id;   /* Optional correlation label. */
    const agent_limits_t* limits;   /* NULL inherits; otherwise a complete override. */
    void* user_data;                /* Application context forwarded to tool/context/policy. */
} agent_request_t;

/* Synchronous result, separating execution from text delivery. */
typedef struct {
    char* output;                /* BORROWED writable destination. */
    size_t output_size;          /* Capacity including the terminator. */
    size_t output_written;       /* Copied bytes excluding terminator. */
    size_t output_required;      /* Full final byte length excluding terminator. */
    agent_error_t status;  /* Execution status, independent of delivery_status. */
    agent_error_t delivery_status; /* AGENT_OK or AGENT_ERROR_TRUNCATED. */
    bool output_truncated;       /* True when the complete final was not delivered. */
    agent_run_summary_t summary; /* Per-turn execution facts. */
    agent_stats_t stats;         /* Cumulative snapshot after this run. */
} agent_response_t;

#ifdef __cplusplus
}
#endif
