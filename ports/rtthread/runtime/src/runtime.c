/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 tanglinjie */
#include <agent_rtthread_runtime.h>
#include "core/text_internal.h"
#include <limits.h>

#if RTTHREAD_VERSION < RT_VERSION_CHECK(5, 1, 0)
#error The RT-Thread Runtime port requires RT-Thread 5.1 or later
#endif
#if RT_TICK_PER_SECOND < 1
#error RT_TICK_PER_SECOND must be positive
#endif
typedef char tick_must_be_32_bits[(sizeof(rt_tick_t) * CHAR_BIT == 32u) ? 1 : -1];

static uint64_t sample(agent_rtthread_runtime_t* state)
{
    rt_base_t level = rt_spin_lock_irqsave(&state->clock_lock);
    rt_tick_t tick = rt_tick_get();
    uint64_t delta = (rt_tick_t)(tick - state->last_tick);
    uint64_t ticks;
    state->ticks = delta > UINT64_MAX - state->ticks ? UINT64_MAX : state->ticks + delta;
    state->last_tick = tick;
    ticks = state->ticks;
    rt_spin_unlock_irqrestore(&state->clock_lock, level);
    return ticks;
}

static void sampler(void* context) { (void)sample(context); }

static uint64_t now_ms(void* context)
{
    uint64_t ticks = sample(context), seconds = ticks / RT_TICK_PER_SECOND;
    uint64_t fraction = (ticks % RT_TICK_PER_SECOND) * UINT64_C(1000) / RT_TICK_PER_SECOND;
    if (seconds > (UINT64_MAX - fraction) / 1000u) return UINT64_MAX;
    return seconds * 1000u + fraction;
}

static void cancel_enter(void* context)
{
    agent_rtthread_runtime_t* state = context;
    state->cancel_level = rt_spin_lock_irqsave(&state->cancel_lock);
}

static void cancel_leave(void* context)
{
    agent_rtthread_runtime_t* state = context;
    rt_base_t level = state->cancel_level;
    rt_spin_unlock_irqrestore(&state->cancel_lock, level);
}

#ifdef RT_USING_HEAP
static void* allocate(void* context, size_t bytes) { (void)context; return rt_malloc(bytes); }
static void release(void* context, void* memory) { (void)context; rt_free(memory); }
#endif

agent_error_t agent_port_rtthread_runtime_init(agent_runtime_t* out,
    agent_rtthread_runtime_t* state, bool use_heap_allocator)
{
    agent_runtime_t runtime = {0};
    if (!out || !state || agent_bytes_overlap(out, sizeof(*out), state, sizeof(*state)))
        return AGENT_ERROR_INVALID;
    if (state->active) return AGENT_ERROR_BUSY;
#ifndef RT_USING_HEAP
    if (use_heap_allocator) return AGENT_ERROR_NOT_SUPPORTED;
#endif
    rt_spin_lock_init(&state->clock_lock);
    rt_spin_lock_init(&state->cancel_lock);
    state->last_tick = rt_tick_get();
    state->ticks = state->last_tick;
    /* Sampling every quarter-cycle observes wrap even when no Agent is running. */
    rt_timer_init(&state->sampler, "agclock", sampler, state, (rt_tick_t)(UINT32_MAX / 4u),
                  RT_TIMER_FLAG_PERIODIC);
    if (rt_timer_start(&state->sampler) != RT_EOK) {
        rt_timer_detach(&state->sampler);
        return AGENT_ERROR_IO;
    }
    state->active = true;
    runtime.now_ms = now_ms;
    runtime.clock_context = state;
    runtime.cancel_sync = (agent_sync_t){cancel_enter, cancel_leave, state};
#ifdef RT_USING_HEAP
    if (use_heap_allocator) runtime.allocator = (agent_allocator_t){allocate, release, NULL};
#endif
    *out = runtime;
    return AGENT_OK;
}

agent_error_t agent_port_rtthread_runtime_deinit(agent_rtthread_runtime_t* state)
{
    if (!state) return AGENT_ERROR_INVALID;
    if (!state->active) return AGENT_ERROR_STATE;
    if (rt_timer_detach(&state->sampler) != RT_EOK) return AGENT_ERROR_IO;
    state->active = false;
    return AGENT_OK;
}
