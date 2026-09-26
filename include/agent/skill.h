/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* Optional static prompt contributions; loaders remain application-side. Interface draft; declarations do not imply an implemented feature. */
#pragma once

#include <agent/error.h>
#include <agent/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Skill metadata, shallow-copied by registration.  */
typedef struct {
    agent_string_view_t name;        /* Unique nonempty name; borrowed and immutable. */
    agent_string_view_t description; /* Optional borrowed description. */
    agent_string_view_t content;     /* Borrowed UTF-8 prompt fragment. */
    int32_t priority;                /* Higher first; ties follow registration order. */
    bool required;                   /* Overflow fails the turn; otherwise skip the whole skill. */
} agent_skill_t;

/* Register an immutable skill in CONFIGURING/READY. */
agent_error_t agent_register_skill(agent_t* agent, const agent_skill_t* skill);

/* Unregister a skill and release borrowed references. Returns: AGENT_OK; INVALID, NOT_FOUND or BUSY. */
agent_error_t agent_unregister_skill(agent_t* agent, agent_string_view_t name);

#ifdef __cplusplus
}
#endif
