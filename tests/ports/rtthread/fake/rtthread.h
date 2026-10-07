/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 tanglinjie */
#pragma once
#include <stddef.h>
#include <stdint.h>
#define RT_VERSION_CHECK(a,b,c) ((a)*10000u+(b)*100u+(c))
#ifndef RTTHREAD_VERSION
#define RTTHREAD_VERSION RT_VERSION_CHECK(5,1,0)
#endif
#ifndef RT_TICK_PER_SECOND
#define RT_TICK_PER_SECOND 1000
#endif
#define RT_EOK 0
#define RT_TIMER_FLAG_PERIODIC 2
typedef uint32_t rt_tick_t;
typedef intptr_t rt_base_t;
typedef int rt_err_t;
typedef unsigned char rt_bool_t;
struct rt_spinlock { int held; };
struct rt_timer { void (*callback)(void*); void* context; rt_tick_t period; int attached; };
rt_tick_t rt_tick_get(void);
void rt_spin_lock_init(struct rt_spinlock*);
rt_base_t rt_spin_lock_irqsave(struct rt_spinlock*);
void rt_spin_unlock_irqrestore(struct rt_spinlock*, rt_base_t);
void rt_timer_init(struct rt_timer*, const char*, void (*)(void*), void*, rt_tick_t, unsigned char);
rt_err_t rt_timer_start(struct rt_timer*);
rt_err_t rt_timer_detach(struct rt_timer*);
void* rt_malloc(size_t);
void rt_free(void*);
