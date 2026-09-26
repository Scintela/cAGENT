/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* Private checked arithmetic and caller-backed arena. */
#pragma once

#include <agent/error.h>
#include <agent/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AGENT_UNUSED(value) ((void)(value))

typedef struct {
    unsigned char* base; /* NULL while measuring layout. */
    size_t capacity;
    size_t used;
    size_t peak;
    size_t alignment;
} agent_arena_t;

agent_error_t agent_arena_init(agent_arena_t* arena, void* base, size_t capacity);
agent_error_t agent_arena_take(agent_arena_t* arena, size_t size, size_t alignment,
                                    void** memory);
agent_error_t agent_arena_rewind(agent_arena_t* arena, size_t mark);
agent_error_t agent_size_add(size_t left, size_t right, size_t* result);
agent_error_t agent_size_multiply(size_t left, size_t right, size_t* result);

#ifdef __cplusplus
}
#endif
