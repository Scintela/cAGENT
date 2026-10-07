/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 tanglinjie */
#pragma once
#include <agent/runtime.h>
#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Zero-initialize; keep alive and at a stable address until all consumers are destroyed. */
typedef struct {
    struct rt_timer sampler;
    struct rt_spinlock clock_lock;
    struct rt_spinlock cancel_lock;
    rt_base_t cancel_level;
    rt_tick_t last_tick;
    uint64_t ticks;
    bool active;
} agent_rtthread_runtime_t;

/* RT-Thread 5.1+; installs a static periodic sampler, clock and short cancel synchronization. */
agent_error_t agent_port_rtthread_runtime_init(agent_runtime_t* out,
    agent_rtthread_runtime_t* state, bool use_heap_allocator);
/* Task-only; destroy consumers first; SMP/all-soft BSPs must quiesce sampler dispatch before detach. */
agent_error_t agent_port_rtthread_runtime_deinit(agent_rtthread_runtime_t* state);

#ifdef __cplusplus
}
#endif
