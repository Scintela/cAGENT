/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
#include "core/core_internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef TEST_LINK_OPENAI
#include <agent_openai_model.h>
#endif

static uint64_t now;
static uint64_t clock_ms(void* context)
{
    (void)context;
    return now;
}
static agent_error_t empty_handler(void* data, const agent_tool_context_t* context,
                                   const agent_text_sink_t* output)
{
    (void)data;
    (void)context;
    (void)output;
    return AGENT_OK;
}

#if AGENT_MAX_TOOLS > 0
#include "json_internal.h"

typedef struct {
    agent_t* agent;
    agent_policy_decision_t decision;
    agent_error_t validation, handler_status;
    unsigned validates, policies, calls;
    uint64_t advance_validate, advance_policy, advance_handler;
    uint64_t observed_deadline;
    agent_cancel_token_t* token;
    unsigned mode;
    bool reenter;
} fixture_t;

static const agent_string_view_t tool_name = AGENT_SV_LITERAL("switch");

static void check_reentry(fixture_t* fixture)
{
    bool enabled;
    assert(agent_unregister_tool(fixture->agent, tool_name) == AGENT_ERROR_BUSY);
    assert(agent_register_tool(fixture->agent, NULL) == AGENT_ERROR_BUSY);
    assert(agent_tool_set_enabled(fixture->agent, tool_name, true) == AGENT_ERROR_BUSY);
    assert(agent_set_policy_callback(fixture->agent, NULL, NULL) == AGENT_ERROR_BUSY);
    assert(agent_tool_is_enabled(fixture->agent, tool_name, &enabled) == AGENT_ERROR_BUSY);
    assert(agent_start(fixture->agent) == AGENT_ERROR_BUSY);
    agent_destroy(fixture->agent);
    assert(fixture->agent->workspace != NULL);
}

static agent_error_t validate(void* data, const agent_tool_context_t* context)
{
    fixture_t* f = data;
    ++f->validates;
    f->observed_deadline = context->deadline_ms;
    if (f->reenter)
        check_reentry(f);
    now += f->advance_validate;
    return f->validation;
}

static agent_policy_decision_t authorize(void* data, const agent_policy_request_t* request)
{
    fixture_t* f = data;
    assert(request->source == AGENT_CALL_SOURCE_MODEL);
    assert(request->context->request_user_data == f);
    assert(request->tool->user_data == f);
    ++f->policies;
    if (f->reenter)
        check_reentry(f);
    now += f->advance_policy;
    return f->decision;
}

static agent_error_t handler(void* data, const agent_tool_context_t* context,
                             const agent_text_sink_t* sink)
{
    fixture_t* f = data;
    static char excessive[AGENT_MAX_TOOL_OUTPUT_BYTES + 1u];
    ++f->calls;
    assert(context->request_user_data == f);
    if (f->reenter)
        check_reentry(f);
    now += f->advance_handler;
    switch (f->mode)
    {
    case 1:
        memset(excessive, 'x', sizeof(excessive));
        assert(sink->write(sink->context, agent_string_view(excessive, sizeof(excessive))) ==
               AGENT_ERROR_LIMIT);
        assert(sink->write(sink->context, agent_string_view("ok", 2u)) == AGENT_ERROR_LIMIT);
        break;
    case 2:
        assert(sink->write(sink->context, agent_string_view(NULL, 1u)) == AGENT_ERROR_INVALID);
        break;
    case 3:
        assert(sink->write(sink->context, agent_string_view("\xe4", 1u)) == AGENT_OK);
        assert(sink->write(sink->context, agent_string_view("\xb8\xad", 2u)) == AGENT_OK);
        break;
    case 4:
        assert(sink->write(sink->context, agent_string_view("\xc0", 1u)) == AGENT_OK);
        break;
    case 5:
        assert(sink->write(sink->context, agent_string_view("partial", 7u)) == AGENT_OK);
        assert(agent_cancel(f->agent) == AGENT_OK);
        assert(sink->write(sink->context, agent_string_view("more", 4u)) == AGENT_ERROR_CANCELLED);
        break;
    case 6:
        assert(sink->write(sink->context, agent_string_view("a\0b", 3u)) == AGENT_ERROR_INVALID);
        break;
    case 7:
        memset(excessive, 'x', sizeof(excessive));
        assert(sink->write(sink->context,
                           agent_string_view(excessive, AGENT_MAX_TOOL_OUTPUT_BYTES)) == AGENT_OK);
        break;
    case 8:
        assert(sink->write(sink->context, agent_string_view("ok", 2u)) == AGENT_ERROR_TIMEOUT);
        break;
    case 9:
        break;
    default:
        assert(sink->write(sink->context, agent_string_view("ok", 2u)) == AGENT_OK);
    }
    return f->handler_status;
}

static agent_tool_t definition(fixture_t* f)
{
    agent_tool_t t = {0};
    t.name = tool_name;
    t.description = agent_string_view("Switch device state", 19u);
    t.input_schema_json = agent_string_view("{\"type\":\"object\"}", 17u);
    t.flags = AGENT_TOOL_SIDE_EFFECT;
    t.validate = validate;
    t.execute = handler;
    t.user_data = f;
    return t;
}

static agent_error_t visit(void* data, const agent_tool_t* tool)
{
    fixture_t* f = data;
    assert(agent_tool_valid_name(tool->name));
    ++f->calls;
    assert(agent_tool_enumerate(f->agent, visit, f) == AGENT_ERROR_BUSY);
    check_reentry(f);
    return f->handler_status;
}

static agent_error_t invoke(fixture_t* f, agent_tool_context_t* context, uint32_t remaining,
                            char* output, size_t size, agent_tool_execution_t* result)
{
    agent_error_t status;
    f->agent->state = AGENT_CORE_ACTIVE;
    status = agent_tool_invoke(f->agent, context, remaining, output, size, result);
    assert(!f->agent->in_callback);
    f->agent->state = AGENT_CORE_READY;
    return status;
}

static void json_admission(void)
{
    const char* bad[] = {"{\"a\":1,\"a\":2}",
                         "{\"a\":1,\"\\u0061\":2}",
                         "{\"x\":[{\"a\":1,\"a\":2}]}",
                         "{\"\\u0000\":1}",
                         "{\"\xe4\xb8\xad\":1,\"\\u4e2d\":2}",
                         "{\"\\uD83D\\uDE00\":1,\"\xf0\x9f\x98\x80\":2}",
                         "{\"/\":1,\"\\/\":2}",
                         "{\"a\":01}",
                         "{\"a\":NaN}",
                         "{\"a\":1,}",
                         "{\"a\":\"\\uD800\"}",
                         "{}{}",
                         "[]"};
    size_t i;
    for (i = 0u; i < sizeof(bad) / sizeof(bad[0]); ++i)
        assert(agent_json_validate_unique_object(agent_string_view(bad[i], strlen(bad[i])), 16u) ==
               AGENT_ERROR_PARSE);
    assert(agent_json_validate_unique_object(
               agent_string_view("{\"a\":[{\"x\":1},{\"x\":2}]}", 23u), 16u) == AGENT_OK);
    assert(agent_json_validate_unique_object(agent_string_view("{\"a\":{}}", 8u), 1u) ==
           AGENT_ERROR_LIMIT);
    {
        const char* good[] = {"{}",
                              " { \"a\" : [1,2], \"aa\":{\"a\":3}, \"b\":null } ",
                              "{\"a\":1,\"ab\":2}",
                              "{\"ab\":1,\"a\":2}",
                              "{\"a\":\"\\u0000\",\"b\":false}",
                              "{\"\":1,\"a\":2}"};
        for (i = 0u; i < sizeof(good) / sizeof(good[0]); ++i)
            assert(agent_json_validate_unique_object(agent_string_view(good[i], strlen(good[i])),
                                                     16u) == AGENT_OK);
    }
    /* Deterministic malformed-input smoke check; not a substitute for a fuzzing campaign. */
    {
        uint32_t random = 1u;
        char bytes[128];
        unsigned round;
        for (round = 0u; round < 4096u; ++round)
        {
            size_t length = round % sizeof(bytes) + 1u;
            for (i = 0u; i < length; ++i)
            {
                random = random * 1664525u + 1013904223u;
                bytes[i] = (char)(random >> 24);
            }
            (void)agent_json_validate_unique_object(agent_string_view(bytes, length), 16u);
        }
    }
}

static void run_contract(agent_t* agent)
{
    fixture_t f = {0};
    agent_tool_t t, extra;
    agent_tool_context_t context = {0};
    agent_limits_t limits = AGENT_LIMITS_DEFAULT;
    agent_tool_execution_t result;
    agent_tool_view_t views[AGENT_MAX_TOOLS];
    agent_cancel_token_t token;
    char output[AGENT_MAX_TOOL_OUTPUT_BYTES + 1u];
    char names[AGENT_MAX_TOOLS][24];
    char large_schema[AGENT_MAX_SCHEMA_BYTES + 1u];
    size_t count, i;
    bool enabled;

    f.agent = agent;
    f.decision = AGENT_POLICY_ALLOW;
    t = definition(&f);
    assert(agent->tools && agent->scratch.base > (unsigned char*)agent->tools);
    assert(agent_register_tool(agent, &t) == AGENT_OK);
    assert(agent_register_tool(agent, &t) == AGENT_ERROR_EXISTS);
    t.name = agent_string_view("bad.name", 8u);
    assert(agent_register_tool(agent, &t) == AGENT_ERROR_INVALID);
    t = definition(&f);
    t.name = agent_string_view("invalid", 7u);
    t.flags |= AGENT_TOOL_READ_ONLY;
    assert(agent_register_tool(agent, &t) == AGENT_ERROR_INVALID);
    t.flags = 1u << 31;
    assert(agent_register_tool(agent, &t) == AGENT_ERROR_INVALID);
    t.flags = 0u;
    t.execute = NULL;
    assert(agent_register_tool(agent, &t) == AGENT_ERROR_INVALID);
    t.execute = handler;
    t.input_schema_json = agent_string_view("[]", 2u);
    assert(agent_register_tool(agent, &t) == AGENT_ERROR_PARSE);
    t.input_schema_json = agent_string_view("{\"a\":1,\"a\":2}", 13u);
    assert(agent_register_tool(agent, &t) == AGENT_ERROR_PARSE);
    t.input_schema_json = agent_string_view(large_schema, sizeof(large_schema));
    assert(agent_register_tool(agent, &t) == AGENT_ERROR_LIMIT);
    t = definition(&f);
    t.name = agent_string_view("invalid", 7u);
    t.description = agent_string_view("\xff", 1u);
    assert(agent_register_tool(agent, &t) == AGENT_ERROR_PARSE);
    t.description = agent_string_view((const char*)agent->scratch.base, 1u);
    assert(agent_register_tool(agent, &t) == AGENT_ERROR_INVALID);
    assert(agent->tools->count == 1u);
    for (i = 1u; i < AGENT_MAX_TOOLS; ++i)
    {
        int length = snprintf(names[i], sizeof(names[i]), "tool_%u", (unsigned)i);
        extra = definition(&f);
        extra.name = agent_string_view(names[i], (size_t)length);
        assert(agent_register_tool(agent, &extra) == AGENT_OK);
    }
    extra = definition(&f);
    extra.name = agent_string_view("overflow", 8u);
    assert(agent_register_tool(agent, &extra) == AGENT_ERROR_CAPACITY);
    assert(agent_tool_registry_project(agent->tools, views, AGENT_MAX_TOOLS - 1u, &count) ==
           AGENT_ERROR_CAPACITY);
    assert(count == 0u);
    assert(agent_tool_registry_project(agent->tools, views, AGENT_MAX_TOOLS, &count) == AGENT_OK);
    assert(count == AGENT_MAX_TOOLS && views[0].name.data == tool_name.data);
    assert(agent_tool_set_enabled(agent, tool_name, false) == AGENT_OK);
    assert(agent_tool_is_enabled(agent, tool_name, &enabled) == AGENT_OK && !enabled);
    assert(agent_tool_registry_project(agent->tools, views, AGENT_MAX_TOOLS, &count) == AGENT_OK &&
           count == AGENT_MAX_TOOLS - 1u);
    assert(agent_tool_set_enabled(agent, tool_name, true) == AGENT_OK);
    assert(agent_tool_enumerate(agent, visit, &f) == AGENT_OK && f.calls == AGENT_MAX_TOOLS);
    f.calls = 0u;
    f.handler_status = AGENT_ERROR_IO;
    assert(agent_tool_enumerate(agent, visit, &f) == AGENT_ERROR_IO && f.calls == 1u);
    f.calls = 0u;
    f.handler_status = AGENT_OK;
    for (i = 1u; i < AGENT_MAX_TOOLS; ++i)
        assert(agent_unregister_tool(agent, agent_string_view(names[i], strlen(names[i]))) ==
               AGENT_OK);
    assert(agent_unregister_tool(agent, agent_string_view("absent", 6u)) == AGENT_ERROR_NOT_FOUND);

    context.call = (agent_tool_call_view_t){AGENT_SV_LITERAL("id1"), AGENT_SV_LITERAL("switch"),
                                            AGENT_SV_LITERAL("{}")};
    context.limits = &limits;
    context.request_user_data = &f;
    assert(invoke(&f, &context, 1u, output, sizeof(output), &result) == AGENT_ERROR_POLICY_DENIED);
    assert(!result.handler_called && f.calls == 0u);
    assert(agent_set_policy_callback(agent, authorize, &f) == AGENT_OK);
    f.reenter = true;
    assert(invoke(&f, &context, 1u, output, sizeof(output), &result) == AGENT_OK);
    assert(result.handler_called && result.handler_status == AGENT_OK && result.output.size == 2u &&
           !strcmp(output, "ok"));
    f.reenter = false;
    assert(invoke(&f, &context, 0u, output, sizeof(output), &result) == AGENT_ERROR_LIMIT &&
           !result.handler_called);
    limits.max_tool_calls = 0u;
    assert(invoke(&f, &context, 1u, output, sizeof(output), &result) == AGENT_ERROR_LIMIT);
    limits.max_tool_calls = 4u;
    assert(invoke(&f, &context, 1u, output, sizeof(output) - 1u, &result) == AGENT_ERROR_CAPACITY &&
           !result.handler_called);
    context.call.arguments_json = agent_string_view("{\"a\":1,\"\\u0061\":2}", 18u);
    assert(invoke(&f, &context, 1u, output, sizeof(output), &result) == AGENT_ERROR_TOOL_ARGUMENT &&
           !result.handler_called);
    context.call.arguments_json = agent_string_view(large_schema, AGENT_MAX_ARGUMENTS_BYTES + 1u);
    assert(invoke(&f, &context, 1u, output, sizeof(output), &result) == AGENT_ERROR_LIMIT);
    context.call.arguments_json = agent_string_view("{}", 2u);
    f.validation = AGENT_ERROR_INVALID;
    assert(invoke(&f, &context, 1u, output, sizeof(output), &result) == AGENT_ERROR_TOOL_ARGUMENT);
    f.validation = AGENT_OK;
    f.decision = AGENT_POLICY_CONFIRM;
    assert(invoke(&f, &context, 1u, output, sizeof(output), &result) == AGENT_ERROR_POLICY_DENIED);
    f.decision = (agent_policy_decision_t)99;
    assert(invoke(&f, &context, 1u, output, sizeof(output), &result) == AGENT_ERROR_POLICY_DENIED);
    f.decision = AGENT_POLICY_ALLOW;
    assert(agent_unregister_tool(agent, tool_name) == AGENT_OK);
    t = definition(&f);
    t.flags |= AGENT_TOOL_REQUIRES_CONFIRM;
    assert(agent_register_tool(agent, &t) == AGENT_OK);
    assert(invoke(&f, &context, 1u, output, sizeof(output), &result) == AGENT_ERROR_POLICY_DENIED);
    assert(agent_unregister_tool(agent, tool_name) == AGENT_OK);
    t.flags = AGENT_TOOL_HIDDEN;
    assert(agent_register_tool(agent, &t) == AGENT_OK);
    assert(agent_tool_registry_project(agent->tools, views, AGENT_MAX_TOOLS, &count) == AGENT_OK &&
           count == 0u);
    assert(agent_tool_is_enabled(agent, tool_name, &enabled) == AGENT_OK && enabled);
    assert(invoke(&f, &context, 1u, output, sizeof(output), &result) == AGENT_ERROR_POLICY_DENIED);
    assert(agent_unregister_tool(agent, tool_name) == AGENT_OK);
    t.flags = AGENT_TOOL_DISABLED;
    assert(agent_register_tool(agent, &t) == AGENT_OK);
    assert(invoke(&f, &context, 1u, output, sizeof(output), &result) == AGENT_ERROR_POLICY_DENIED);
    assert(agent_tool_set_enabled(agent, tool_name, true) == AGENT_OK);

    for (i = 1u; i <= 7u; ++i)
    {
        const agent_error_t expected[] = {
            AGENT_OK,          AGENT_ERROR_LIMIT,     AGENT_ERROR_INVALID, AGENT_OK,
            AGENT_ERROR_PARSE, AGENT_ERROR_CANCELLED, AGENT_ERROR_INVALID, AGENT_OK};
        f.mode = (unsigned)i;
        agent_cancel_token_init(&token, NULL);
        context.cancel = &token;
        agent->active_cancel = &token;
        assert(invoke(&f, &context, 1u, output, sizeof(output), &result) == expected[i]);
        assert(result.handler_called && result.handler_status == AGENT_OK);
        if (i == 7u)
            assert(result.output.size == AGENT_MAX_TOOL_OUTPUT_BYTES &&
                   output[result.output.size] == '\0');
    }
    agent->active_cancel = NULL;
    context.cancel = NULL;
    f.mode = 0u;
    f.handler_status = AGENT_ERROR;
    assert(invoke(&f, &context, 1u, output, sizeof(output), &result) == AGENT_ERROR_TOOL_FAILED);
    f.handler_status = AGENT_ERROR_IO;
    assert(invoke(&f, &context, 1u, output, sizeof(output), &result) == AGENT_ERROR_IO);
    f.handler_status = AGENT_OK;
    now = 10u;
    context.deadline_ms = 10u;
    assert(invoke(&f, &context, 1u, output, sizeof(output), &result) == AGENT_ERROR_TIMEOUT &&
           !result.handler_called);
    context.deadline_ms = 0u;
    limits.per_tool_timeout_ms = 2u;
    f.advance_validate = 2u;
    assert(invoke(&f, &context, 1u, output, sizeof(output), &result) == AGENT_ERROR_TIMEOUT &&
           !result.handler_called);
    f.advance_validate = 0u;
    f.advance_policy = 2u;
    assert(invoke(&f, &context, 1u, output, sizeof(output), &result) == AGENT_ERROR_TIMEOUT &&
           !result.handler_called);
    f.advance_policy = 0u;
    f.advance_handler = 2u;
    f.mode = 8u;
    assert(invoke(&f, &context, 1u, output, sizeof(output), &result) == AGENT_ERROR_TIMEOUT &&
           result.handler_called);
    f.mode = 9u;
    assert(invoke(&f, &context, 1u, output, sizeof(output), &result) == AGENT_ERROR_TIMEOUT &&
           result.handler_called);
    f.advance_handler = 0u;
    f.mode = 0u;
    now = UINT64_MAX - 1u;
    assert(invoke(&f, &context, 1u, output, sizeof(output), &result) == AGENT_OK &&
           f.observed_deadline == UINT64_MAX);
    now = 10u;
    context.deadline_ms = 11u;
    assert(invoke(&f, &context, 1u, output, sizeof(output), &result) == AGENT_OK &&
           f.observed_deadline == 11u);
    context.deadline_ms = 0u;
    agent_cancel_token_init(&token, NULL);
    agent_cancel_token_request_locked(&token);
    context.cancel = &token;
    assert(invoke(&f, &context, 1u, output, sizeof(output), &result) == AGENT_ERROR_CANCELLED &&
           !result.handler_called);
    context.cancel = NULL;
    context.call.name = agent_string_view("absent", 6u);
    assert(invoke(&f, &context, 1u, output, sizeof(output), &result) == AGENT_ERROR_NOT_FOUND);
    context.call.name = tool_name;
    assert(invoke(&f, &context, 1u, (char*)context.call.arguments_json.data, sizeof(output),
                  &result) == AGENT_ERROR_INVALID);
    assert(invoke(&f, &context, 1u, (char*)agent, sizeof(output), &result) == AGENT_ERROR_INVALID);
    assert(invoke(&f, &context, 1u, output, sizeof(output), (agent_tool_execution_t*)output) ==
           AGENT_ERROR_INVALID);
    assert(agent_tool_is_enabled(agent, tool_name, (bool*)agent) == AGENT_ERROR_INVALID);
    assert(agent_tool_registry_project(agent->tools, views, SIZE_MAX, &count) ==
           AGENT_ERROR_INVALID);
    assert(agent_tool_registry_project(agent->tools, (agent_tool_view_t*)agent->tools, 1u,
                                       &count) == AGENT_ERROR_INVALID);
    agent->state = AGENT_CORE_ACTIVE;
    assert(agent_unregister_tool(agent, tool_name) == AGENT_ERROR_BUSY);
    agent->state = AGENT_CORE_READY;
    assert(agent_tool_invoke(agent, &context, 1u, output, sizeof(output), &result) ==
           AGENT_ERROR_STATE);
    /* Invocation buffers may come from Core scratch, not just external RAM. */
    {
        void *storage, *facts;
        assert(agent_arena_take(&agent->scratch, sizeof(output), 1u, &storage) == AGENT_OK);
        assert(agent_arena_take(&agent->scratch, sizeof(result),
                                AGENT_ALIGNOF(agent_tool_execution_t), &facts) == AGENT_OK);
        assert(invoke(&f, &context, 1u, storage, sizeof(output), facts) == AGENT_OK);
    }
    assert(agent_set_policy_callback(agent, NULL, &f) == AGENT_OK && agent->policy_context == NULL);
    assert(agent_unregister_tool(agent, tool_name) == AGENT_OK && agent->tools->count == 0u);
}
#endif

int main(void)
{
    static agent_workspace_t workspace;
    agent_t* agent;
    agent_config_t config = agent_config_default();
    config.runtime.now_ms = clock_ms;
    assert(agent_init(&agent, &workspace, &config) == AGENT_OK);
#ifdef TEST_LINK_OPENAI
    assert(agent_openai_model_ops()->complete != NULL);
#endif
#if AGENT_MAX_TOOLS > 0
    (void)empty_handler;
    json_admission();
    run_contract(agent);
#else
    agent_tool_t tool = {0};
    bool enabled;
    tool.name = agent_string_view("empty", 5u);
    tool.input_schema_json = agent_string_view("{}", 2u);
    tool.execute = empty_handler;
    assert(agent->tools == NULL);
    assert(agent_register_tool(agent, &tool) == AGENT_ERROR_NOT_SUPPORTED);
    assert(agent_tool_is_enabled(agent, tool.name, &enabled) == AGENT_ERROR_NOT_SUPPORTED);
    assert(agent_unregister_tool(agent, tool.name) == AGENT_ERROR_NOT_SUPPORTED);
#endif
    agent_destroy(agent);
    puts("PASS: Tool registry and synchronous safety pipeline");
    return 0;
}
