/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* Cooperative cancellation for synchronous agent_run and provider/tool callbacks. */
#pragma once

#include <agent/error.h>
#include <agent/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Polls an active-run token. Not ISR-safe; cross-task use needs runtime synchronization. */
bool agent_cancel_token_is_set(const agent_cancel_token_t* token);

/* Request cancellation of the active run; idle calls are no-ops. Not ISR-safe. */
agent_error_t agent_cancel(agent_t* agent);

#ifdef __cplusplus
}
#endif
