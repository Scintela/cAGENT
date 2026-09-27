/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* Clock-only OpenVela Runtime contract using a deterministic POSIX clock. */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <agent_openvela_runtime.h>

#include <stdint.h>
#include <time.h>

static int fake_clock_error;
static time_t fake_seconds = 12;
static long fake_nanoseconds = 345000000L;

int clock_gettime(clockid_t clock_id, struct timespec* result)
{
    if (clock_id != CLOCK_MONOTONIC || fake_clock_error != 0)
    {
        return -1;
    }
    result->tv_sec = fake_seconds;
    result->tv_nsec = fake_nanoseconds;
    return 0;
}

int main(void)
{
    agent_runtime_t runtime = {0};

    if (agent_port_openvela_runtime_init(NULL) != AGENT_ERROR_INVALID)
    {
        return 1;
    }
    fake_clock_error = 1;
    if (agent_port_openvela_runtime_init(&runtime) != AGENT_ERROR_IO ||
        runtime.now_ms != NULL)
    {
        return 2;
    }
    fake_clock_error = 0;
    if (agent_port_openvela_runtime_init(&runtime) != AGENT_OK ||
        runtime.now_ms == NULL || runtime.allocator.alloc != NULL ||
        runtime.cancel_sync.enter != NULL || runtime.log != NULL ||
        runtime.now_ms(runtime.clock_context) != 12345u)
    {
        return 3;
    }
    fake_seconds += 1;
    if (runtime.now_ms(runtime.clock_context) != 13345u)
    {
        return 4;
    }
    fake_clock_error = 1;
    if (runtime.now_ms(runtime.clock_context) != UINT64_MAX)
    {
        return 5;
    }
    return 0;
}
