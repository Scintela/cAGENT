/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 tanglinjie */
/* Minimal uniprocessor header-check profile, not a deployable BSP configuration. */
#pragma once
#define RT_USING_NANO
#define RT_NAME_MAX 8
#define RT_ALIGN_SIZE 8
#define RT_THREAD_PRIORITY_MAX 32
#define RT_TICK_PER_SECOND 1000
#define RT_TIMER_SKIP_LIST_LEVEL 1
#define RT_USING_HEAP
#define RT_USING_LIBC
