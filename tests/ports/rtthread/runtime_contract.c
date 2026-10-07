/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 tanglinjie */
#include <agent_rtthread_runtime.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static rt_tick_t tick;
static int start_error, detach_error, locks, unlocks;
rt_tick_t rt_tick_get(void) { return tick; }
void rt_spin_lock_init(struct rt_spinlock* lock) { lock->held = 0; }
rt_base_t rt_spin_lock_irqsave(struct rt_spinlock* lock)
{ assert(!lock->held); lock->held = 1; ++locks; return 7; }
void rt_spin_unlock_irqrestore(struct rt_spinlock* lock, rt_base_t level)
{ assert(lock->held && level == 7); lock->held = 0; ++unlocks; }
void rt_timer_init(struct rt_timer* t, const char* name, void (*cb)(void*), void* ctx, rt_tick_t period, unsigned char flags)
{ assert(!strcmp(name, "agclock") && period == UINT32_MAX / 4u && flags == RT_TIMER_FLAG_PERIODIC);
  t->callback = cb; t->context = ctx; t->period = period; t->attached = 1; }
rt_err_t rt_timer_start(struct rt_timer* t) { assert(t->attached); return start_error; }
rt_err_t rt_timer_detach(struct rt_timer* t) { if (!detach_error) t->attached = 0; return detach_error; }
void* rt_malloc(size_t n) { return malloc(n); }
void rt_free(void* p) { free(p); }
int main(void)
{
    agent_rtthread_runtime_t state = {0};
    agent_runtime_t runtime = {0};
    uint64_t expected;
    unsigned i;
    assert(agent_port_rtthread_runtime_init(NULL, &state, false) == AGENT_ERROR_INVALID);
    assert(agent_port_rtthread_runtime_init((agent_runtime_t*)&state, &state, false) == AGENT_ERROR_INVALID);
    assert(agent_port_rtthread_runtime_deinit(&state) == AGENT_ERROR_STATE);
    start_error = -1;
    assert(agent_port_rtthread_runtime_init(&runtime, &state, false) == AGENT_ERROR_IO);
    assert(!state.active && !state.sampler.attached && !runtime.now_ms);
    start_error = 0; tick = UINT32_MAX - 10u;
    assert(agent_port_rtthread_runtime_init(&runtime, &state, false) == AGENT_OK);
    assert(!runtime.allocator.alloc && runtime.now_ms);
    assert(agent_port_rtthread_runtime_init(&runtime, &state, false) == AGENT_ERROR_BUSY);
    expected = tick;
    assert(runtime.now_ms(runtime.clock_context) == expected * 1000u / RT_TICK_PER_SECOND);
    tick = 20u; expected += 31u;
    assert(runtime.now_ms(runtime.clock_context) == expected * 1000u / RT_TICK_PER_SECOND);
    for (i = 0u; i < 10u; ++i) {
        tick += state.sampler.period; expected += state.sampler.period;
        state.sampler.callback(state.sampler.context);
    }
    assert(runtime.now_ms(runtime.clock_context) == expected * 1000u / RT_TICK_PER_SECOND);
    runtime.cancel_sync.enter(runtime.cancel_sync.context);
    runtime.cancel_sync.leave(runtime.cancel_sync.context);
    assert(locks == unlocks);
    detach_error = -1;
    assert(agent_port_rtthread_runtime_deinit(&state) == AGENT_ERROR_IO && state.active);
    detach_error = 0;
    assert(agent_port_rtthread_runtime_deinit(&state) == AGENT_OK);
#ifdef RT_USING_HEAP
    assert(agent_port_rtthread_runtime_init(&runtime, &state, true) == AGENT_OK);
    { void* p = runtime.allocator.alloc(runtime.allocator.context, 16u);
      assert(p); runtime.allocator.free(runtime.allocator.context, p); }
    assert(agent_port_rtthread_runtime_deinit(&state) == AGENT_OK);
#else
    assert(agent_port_rtthread_runtime_init(&runtime, &state, true) == AGENT_ERROR_NOT_SUPPORTED);
#endif
    return 0;
}
