/* SPDX-License-Identifier: MIT */
#include <agent.h>
#include <agent/model.h>

#include "core/arena_internal.h"
#include "core/agent_internal.h"

#include <stdlib.h>

static uint64_t test_now_ms(void* context)
{
    (void)context;
    return 1u;
}

static void* test_alloc(void* context, size_t size)
{
    (void)context;
    return malloc(size);
}

static void test_free(void* context, void* memory)
{
    (void)context;
    free(memory);
}

static unsigned int destroyed_models;
static unsigned int sync_enters;
static unsigned int sync_leaves;

static void test_sync_enter(void* context)
{
    (void)context;
    ++sync_enters;
}

static void test_sync_leave(void* context)
{
    (void)context;
    ++sync_leaves;
}

static void test_destroy(void* context)
{
    (void)context;
    ++destroyed_models;
}

static agent_error_t test_complete(void* context, const agent_model_request_t* request,
                                   const agent_model_sink_t* sink)
{
    (void)context;
    (void)request;
    (void)sink;
    return AGENT_OK;
}

int main(void)
{
    static agent_workspace_t workspace;
    static agent_model_workspace_t model_workspace;
    agent_config_t config = agent_config_default();
    agent_model_ops_t ops = {test_complete, test_destroy};
    agent_model_t* model = NULL;
    agent_model_t* owned_model;
    agent_t* agent = NULL;
    agent_t* heap_agent;
    agent_limits_t invalid_limits = AGENT_LIMITS_DEFAULT;
    agent_response_t response;
    agent_stats_t stats;
    agent_cancel_token_t token;
    unsigned char arena_bytes[8];
    agent_arena_t arena;
    void* arena_memory;
    agent_request_t request = {AGENT_SV_LITERAL(""), AGENT_SV_LITERAL("input"),
                               AGENT_SV_LITERAL(""), NULL, NULL};

    config.runtime.now_ms = test_now_ms;
    config.runtime.allocator.alloc = test_alloc;
    config.runtime.allocator.free = test_free;
    config.runtime.cancel_sync.enter = test_sync_enter;
    config.runtime.cancel_sync.leave = test_sync_leave;
    if (agent_arena_init(&arena, arena_bytes, sizeof(arena_bytes)) != AGENT_OK ||
        agent_arena_take(&arena, sizeof(arena_bytes), 1u, &arena_memory) != AGENT_OK ||
        agent_arena_take(&arena, 1u, 1u, &arena_memory) != AGENT_ERROR_CAPACITY ||
        agent_error_str(AGENT_ERROR_CAPACITY) == NULL)
    {
        return 10;
    }
    if (agent_init(&agent, &workspace, &config) != AGENT_OK || agent == NULL)
    {
        return 1;
    }
    if (agent_start(agent) != AGENT_OK)
    {
        return 2;
    }
    if (agent_cancel(NULL) != AGENT_ERROR_INVALID || agent_cancel(agent) != AGENT_OK ||
        sync_enters != 1u || sync_leaves != 1u)
    {
        return 11;
    }
    agent_cancel_token_init(&token, &config.runtime.cancel_sync);
    agent->active_cancel = &token;
    if (agent_cancel(agent) != AGENT_OK || !agent_cancel_token_is_set(&token) ||
        sync_enters != 3u || sync_leaves != 3u)
    {
        return 12;
    }
    agent->active_cancel = NULL;
    if (agent_set_event_callback(agent, NULL, NULL) != AGENT_OK ||
        agent_get_stats(agent, &stats) != AGENT_OK)
    {
        return 7;
    }
    if (agent_model_init(&model, &model_workspace, &ops, NULL) != AGENT_OK ||
        agent_set_model(agent, model) != AGENT_OK || agent_get_model(agent) != model)
    {
        return 3;
    }
    invalid_limits.max_steps = 0u;
    if (agent_set_limits(agent, &invalid_limits) != AGENT_ERROR_INVALID)
    {
        return 4;
    }
    if (agent_run(agent, &request, &response) != AGENT_ERROR_NOT_SUPPORTED ||
        response.status != AGENT_ERROR_NOT_SUPPORTED)
    {
        return 5;
    }
    agent_destroy(agent);
    agent_model_destroy(model);

    heap_agent = agent_create(&config);
    if (heap_agent == NULL)
    {
        return 6;
    }
    owned_model = agent_model_create(&ops, NULL, &config.runtime.allocator);
    if (owned_model == NULL || agent_set_model_owned(heap_agent, owned_model) != AGENT_OK)
    {
        return 8;
    }
    agent_destroy(heap_agent);
    if (destroyed_models != 2u)
    {
        return 9;
    }
    return 0;
}
