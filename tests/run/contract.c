/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 tanglinjie */
#include "core/core_internal.h"
#include <agent.h>
#include <agent/model.h>
#include <agent_session_ram.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

enum scenario {
    FINAL,
    EMPTY,
    TOOL,
    TWO_TOOLS,
    UNKNOWN,
    BAD_ARGS,
    DUPLICATE,
    BAD_TEXT,
    TOO_BIG,
    MODEL_FAIL,
    IGNORE_SINK,
    MODEL_TIMEOUT,
    CANCEL,
    AGAIN
};
typedef struct {
    agent_workspace_t workspace;
    agent_model_workspace_t model_workspace;
    agent_t* agent;
    agent_model_t* model;
    agent_config_t config;
    agent_request_t request;
    agent_response_t response;
    char output[128];
    uint64_t now;
    enum scenario scenario;
    unsigned models, handlers, events, begins, appends, finishes, reads, contexts;
    unsigned fail_append;
    agent_error_t begin_error, finish_error, handler_error;
    bool tool_timeout, allow, expect_history, contributions;
    agent_event_type_t sequence[64];
    agent_session_turn_outcome_t outcome;
    agent_session_storage_t storage, raw;
    agent_session_ram_t ram;
    agent_session_ram_turn_t turns[8];
    agent_session_ram_message_t messages[64];
    agent_session_ram_call_t calls[16];
    char payload[8192];
    agent_message_view_t read_messages[32];
    agent_tool_call_view_t read_calls[8];
} fixture_t;

static agent_string_view_t sv(const char* s)
{
    return agent_string_view(s, strlen(s));
}
static bool eq(agent_string_view_t s, const char* t)
{
    return s.size == strlen(t) && !memcmp(s.data, t, s.size);
}
static uint64_t clock_now(void* context)
{
    return ((fixture_t*)context)->now;
}
static pthread_mutex_t cancel_mutex = PTHREAD_MUTEX_INITIALIZER;
static void enter(void* context)
{
    (void)context;
    assert(!pthread_mutex_lock(&cancel_mutex));
}
static void leave(void* context)
{
    (void)context;
    assert(!pthread_mutex_unlock(&cancel_mutex));
}
static void* cancel_worker(void* context)
{
    assert(agent_cancel(context) == AGENT_OK);
    return NULL;
}

static void no_reentry(fixture_t* f)
{
    agent_response_t response = {0};
    assert(agent_run(f->agent, &f->request, &response) == AGENT_ERROR_BUSY);
    assert(agent_set_limits(f->agent, &f->config.limits) == AGENT_ERROR_BUSY);
}

static void event(void* data, const agent_event_t* e)
{
    fixture_t* f = data;
    assert(eq(e->session_id, "default"));
    assert(f->events < 64u);
    f->sequence[f->events++] = e->type;
    no_reentry(f);
}

static agent_error_t complete(void* data, const agent_model_request_t* request,
                              const agent_model_sink_t* sink)
{
    fixture_t* f = data;
    agent_tool_call_view_t call = {AGENT_SV_LITERAL("call-1"), AGENT_SV_LITERAL("switch"),
                                   AGENT_SV_LITERAL("{}")};
    char borrowed[32] = "answer";
    agent_error_t status;
    ++f->models;
    no_reentry(f);
    assert(request->cancel && !agent_cancel_token_is_set(request->cancel));
    assert(eq(request->session_id, "default"));
    assert(request->message_count > 0u);
    if (f->expect_history)
        assert(request->message_count == 3u);
    if (f->contributions)
    {
        assert(eq(request->system_prompt, "skill\n\nstate"));
        assert(eq(request->messages[0].content, "memory"));
        assert(f->reads == 1u && f->contexts == f->models);
    }
    if (f->scenario == MODEL_TIMEOUT)
    {
        f->now += f->config.limits.per_model_timeout_ms;
        return AGENT_OK;
    }
    if (f->scenario == CANCEL)
    {
        pthread_t worker;
        assert(!pthread_create(&worker, NULL, cancel_worker, f->agent));
        assert(!pthread_join(worker, NULL));
        return AGENT_OK;
    }
    if (f->scenario == EMPTY)
        return AGENT_OK;
    if (f->scenario == BAD_TEXT)
        return sink->text(sink->context, agent_string_view("\xc0\x80", 2u));
    if (f->scenario == TOO_BIG || f->scenario == IGNORE_SINK)
    {
        static char large[AGENT_MAX_MODEL_OUTPUT_BYTES + 1u];
        memset(large, 'a', sizeof(large));
        status = sink->text(sink->context, agent_string_view(large, sizeof(large)));
        assert(status == AGENT_ERROR_LIMIT);
        assert(sink->text(sink->context, sv("ignored")) == status);
        return f->scenario == IGNORE_SINK ? AGENT_OK : status;
    }
    if (f->scenario == MODEL_FAIL)
    {
        assert(sink->text(sink->context, sv("partial")) == AGENT_OK);
        return AGENT_ERROR_IO;
    }
    if (f->scenario == FINAL)
    {
        assert(sink->text(sink->context, agent_string_view("\xe4", 1u)) == AGENT_OK);
        assert(sink->text(sink->context, agent_string_view("\xb8\xad", 2u)) == AGENT_OK);
        status = sink->text(sink->context, sv(borrowed));
        memset(borrowed, 'x', sizeof(borrowed));
        return status;
    }
    if (f->models == 1u || f->scenario == AGAIN)
    {
        if (f->scenario == UNKNOWN)
            call.name = sv("missing");
        if (f->scenario == BAD_ARGS)
            call.arguments_json = sv("[]");
        assert(sink->text(sink->context, sv("planning")) == AGENT_OK);
        status = sink->tool_call(sink->context, &call);
        if (status != AGENT_OK)
            return status;
        if (f->scenario == DUPLICATE)
            return sink->tool_call(sink->context, &call);
        if (f->scenario == TWO_TOOLS)
        {
            strcpy(borrowed, "call-2");
            call.id = sv(borrowed);
            status = sink->tool_call(sink->context, &call);
            memset(borrowed, 'x', sizeof(borrowed));
        }
        return status;
    }
    {
        const agent_message_view_t* result = &request->messages[request->message_count - 1u];
        assert(result->role == AGENT_MESSAGE_ROLE_TOOL);
        assert(eq(result->tool_call_id, f->scenario == TWO_TOOLS ? "call-2" : "call-1"));
        assert(eq(request->messages[f->contributions ? 2u : 1u].content, "planning"));
        if (f->scenario == UNKNOWN)
            assert(eq(result->content, agent_error_str(AGENT_ERROR_NOT_FOUND)));
        else if (f->scenario == BAD_ARGS)
            assert(eq(result->content, agent_error_str(AGENT_ERROR_TOOL_ARGUMENT)));
        else if (!f->allow)
            assert(eq(result->content, agent_error_str(AGENT_ERROR_POLICY_DENIED)));
        else if (f->handler_error)
            assert(eq(result->content, agent_error_str(f->handler_error)));
        else
            assert(eq(result->content, "done"));
    }
    return sink->text(sink->context, sv("finished"));
}

#if AGENT_MAX_TOOLS > 0
static agent_policy_decision_t policy(void* data, const agent_policy_request_t* request)
{
    fixture_t* f = data;
    assert(eq(request->tool->name, "switch"));
    no_reentry(f);
    return f->allow ? AGENT_POLICY_ALLOW : AGENT_POLICY_DENY;
}
static agent_error_t execute(void* data, const agent_tool_context_t* context,
                             const agent_text_sink_t* sink)
{
    fixture_t* f = data;
    ++f->handlers;
    assert(context->request_user_data == f);
    no_reentry(f);
    if (f->tool_timeout)
        f->now += f->config.limits.per_tool_timeout_ms;
    if (f->handler_error)
        return f->handler_error;
    return sink->write(sink->context, sv("done"));
}
#endif

static agent_error_t begin(void* data, agent_string_view_t id, void** tx)
{
    fixture_t* f = data;
    ++f->begins;
    no_reentry(f);
    return f->begin_error ? f->begin_error : f->raw.ops.begin(f->raw.context, id, tx);
}
static agent_error_t append_message(void* data, void* tx, const agent_message_view_t* message)
{
    fixture_t* f = data;
    ++f->appends;
    no_reentry(f);
    if (f->fail_append == f->appends)
        return AGENT_ERROR_IO;
    return f->raw.ops.append(f->raw.context, tx, message);
}
static agent_error_t finish(void* data, void* tx, agent_session_turn_outcome_t outcome)
{
    fixture_t* f = data;
    agent_error_t result;
    ++f->finishes;
    f->outcome = outcome;
    no_reentry(f);
    result = f->raw.ops.finish(f->raw.context, tx, outcome);
    return f->finish_error ? f->finish_error : result;
}
static agent_error_t recent(void* data, agent_string_view_t id, size_t max,
                            agent_session_visit_fn visit, void* context)
{
    fixture_t* f = data;
    no_reentry(f);
    return f->raw.ops.recent(f->raw.context, id, max, visit, context);
}
static agent_error_t clear(void* data, agent_string_view_t id)
{
    fixture_t* f = data;
    return f->raw.ops.clear(f->raw.context, id);
}
static agent_error_t clear_all(void* data)
{
    fixture_t* f = data;
    return f->raw.ops.clear_all(f->raw.context);
}
static agent_error_t remove_session(void* data, agent_string_view_t id)
{
    fixture_t* f = data;
    return f->raw.ops.remove(f->raw.context, id);
}
static agent_error_t count(void* data, size_t* n)
{
    fixture_t* f = data;
    return f->raw.ops.count(f->raw.context, n);
}

static agent_error_t read_memory(void* data, const agent_memory_key_t* key, char* buffer,
                                 size_t capacity, size_t maximum, size_t* bytes)
{
    fixture_t* f = data;
    assert(key->kind == AGENT_MEMORY_USER && capacity >= 7u && maximum >= 6u);
    ++f->reads;
    no_reentry(f);
    memcpy(buffer, "memory", 7u);
    *bytes = 6u;
    return AGENT_OK;
}
static agent_error_t build_context(void* data, const agent_context_request_t* request,
                                   const agent_text_sink_t* sink)
{
    fixture_t* f = data;
    ++f->contexts;
    no_reentry(f);
    assert(request->request->user_data == f);
    return sink->write(sink->context, sv("state"));
}
static void add_contributions(fixture_t* f)
{
    agent_skill_t skill = {AGENT_SV_LITERAL("skill"), {0}, AGENT_SV_LITERAL("skill"), 0, true};
    agent_memory_t memory = {{read_memory, NULL, NULL}, f};
    agent_memory_context_t source = {
        AGENT_SV_LITERAL("profile"), {AGENT_MEMORY_USER, {0}}, 0, true, 16u};
    agent_context_provider_t dynamic = {AGENT_SV_LITERAL("live"),  0, true, build_context, f, 16u,
                                        AGENT_CONTEXT_INSTRUCTIONS};
    assert(agent_register_skill(f->agent, &skill) == AGENT_OK);
    assert(agent_set_memory(f->agent, &memory) == AGENT_OK);
    assert(agent_register_memory_context(f->agent, &source) == AGENT_OK);
    assert(agent_register_context(f->agent, &dynamic) == AGENT_OK);
    f->contributions = true;
}

static void setup(fixture_t* f, enum scenario scenario)
{
    agent_model_ops_t ops = {complete, NULL};
    agent_session_ram_config_t ram;
    memset(f, 0, sizeof(*f));
    f->now = 10u;
    f->scenario = scenario;
    f->allow = true;
    f->config = agent_config_default();
    f->config.runtime.now_ms = clock_now;
    f->config.runtime.clock_context = f;
    f->config.runtime.cancel_sync = (agent_sync_t){enter, leave, NULL};
    assert(agent_init(&f->agent, &f->workspace, &f->config) == AGENT_OK);
    assert(agent_model_init(&f->model, &f->model_workspace, &ops, f) == AGENT_OK);
    assert(agent_set_model(f->agent, f->model) == AGENT_OK);
    assert(agent_set_event_callback(f->agent, event, f) == AGENT_OK);
#if AGENT_MAX_TOOLS > 0
    {
        agent_tool_t tool = {0};
        tool.name = sv("switch");
        tool.input_schema_json = sv("{}");
        tool.execute = execute;
        tool.user_data = f;
        assert(agent_register_tool(f->agent, &tool) == AGENT_OK);
        assert(agent_set_policy_callback(f->agent, policy, f) == AGENT_OK);
    }
#endif
    ram = (agent_session_ram_config_t){f->turns,         8u,  f->messages,   64u,
                                       f->calls,         16u, f->payload,    sizeof(f->payload),
                                       f->read_messages, 32u, f->read_calls, 8u};
    assert(agent_session_ram_init(&f->ram, &ram) == AGENT_OK);
    assert(agent_session_ram_bind(&f->ram, &f->raw) == AGENT_OK);
    f->storage = (agent_session_storage_t){
        {begin, append_message, finish, recent, clear, clear_all, remove_session, count}, f};
    assert(agent_set_session_storage(f->agent, &f->storage) == AGENT_OK);
    assert(agent_start(f->agent) == AGENT_OK);
    f->request.input = sv("hello");
    f->request.user_data = f;
    f->request.limits = &f->config.limits;
    f->response.output = f->output;
    f->response.output_size = sizeof(f->output);
}

static agent_error_t run(fixture_t* f)
{
    agent_error_t status = agent_run(f->agent, &f->request, &f->response);
    assert(status == f->response.status);
    assert(f->agent->state == AGENT_CORE_READY && !f->agent->in_callback);
    assert(f->agent->scratch.used == 0u && f->agent->active_cancel == NULL);
    assert(f->sequence[0] == AGENT_EVENT_TURN_BEGIN);
    assert(f->sequence[f->events - 1u] == AGENT_EVENT_TURN_END);
    assert(f->response.summary.model_calls == f->models);
    assert(f->response.summary.tool_calls == f->handlers);
    return status;
}

int main(void)
{
    static fixture_t f;
    setup(&f, FINAL);
    assert(run(&f) == AGENT_OK);
    assert(!strcmp(f.output, "\xe4\xb8\xad"
                             "answer"));
    assert(f.response.summary.final_valid && f.events == 4u && f.appends == 2u && f.finishes == 1u);
    assert(f.response.stats.completed_runs == 1u &&
           f.response.summary.scratch_peak_bytes <= AGENT_SCRATCH_BYTES);
    f.models = f.events = f.handlers = 0u;
    f.config.limits.max_history_turns = 1u;
    f.expect_history = true;
    assert(run(&f) == AGENT_OK && f.response.stats.completed_runs == 2u);
    setup(&f, FINAL);
    f.response.output_size = 3u;
    assert(run(&f) == AGENT_OK && f.response.delivery_status == AGENT_ERROR_TRUNCATED);
    assert(f.response.output_written == 0u && f.response.output_required == 9u);
    setup(&f, FINAL);
    f.response.output = NULL;
    f.response.output_size = 0u;
    assert(run(&f) == AGENT_OK && f.response.output_required == 9u && f.response.output_truncated);
    setup(&f, EMPTY);
    assert(run(&f) == AGENT_OK && f.response.summary.final_valid &&
           f.response.output_required == 0u);
    setup(&f, FINAL);
    add_contributions(&f);
    assert(run(&f) == AGENT_OK && f.reads == 1u && f.contexts == 1u);
    setup(&f, BAD_TEXT);
    assert(run(&f) == AGENT_ERROR_MODEL_PARSE && f.outcome == AGENT_SESSION_TURN_ABORTED);
    setup(&f, TOO_BIG);
    assert(run(&f) == AGENT_ERROR_LIMIT);
    setup(&f, IGNORE_SINK);
    assert(run(&f) == AGENT_ERROR_LIMIT);
    setup(&f, MODEL_FAIL);
    assert(run(&f) == AGENT_ERROR_IO && !f.response.summary.final_valid && !f.output[0]);
    setup(&f, MODEL_TIMEOUT);
    assert(run(&f) == AGENT_ERROR_TIMEOUT && f.response.stats.timeout_runs == 1u);
    setup(&f, CANCEL);
    assert(run(&f) == AGENT_ERROR_CANCELLED && f.response.stats.cancelled_runs == 1u);
    setup(&f, FINAL);
    f.begin_error = AGENT_ERROR_IO;
    assert(run(&f) == AGENT_ERROR_IO && f.models == 0u && f.finishes == 0u && f.events == 2u);
    setup(&f, FINAL);
    f.fail_append = 1u;
    assert(run(&f) == AGENT_ERROR_IO && f.models == 0u && f.finishes == 1u);
    setup(&f, FINAL);
    f.fail_append = 2u;
    assert(run(&f) == AGENT_ERROR_IO && f.response.summary.final_valid &&
           f.response.session_status == AGENT_ERROR_IO);
    assert(f.response.output_required == 9u && f.outcome == AGENT_SESSION_TURN_ABORTED);
    setup(&f, FINAL);
    f.finish_error = AGENT_ERROR_IO;
    assert(run(&f) == AGENT_ERROR_IO && f.response.summary.final_valid && f.finishes == 1u);
    setup(&f, CANCEL);
    f.finish_error = AGENT_ERROR_IO;
    assert(run(&f) == AGENT_ERROR_CANCELLED && f.response.session_status == AGENT_ERROR_IO);
    setup(&f, FINAL);
    f.response.output = (char*)f.request.input.data;
    assert(agent_run(f.agent, &f.request, &f.response) == AGENT_ERROR_INVALID && f.events == 0u);
    setup(&f, FINAL);
    f.request.input = agent_string_view((char*)f.agent->scratch.base, 1u);
    assert(agent_run(f.agent, &f.request, &f.response) == AGENT_ERROR_INVALID && f.events == 0u);
    setup(&f, FINAL);
    f.request.input = agent_string_view("x", SIZE_MAX);
    assert(agent_run(f.agent, &f.request, &f.response) != AGENT_OK && f.events == 0u);
    setup(&f, FINAL);
    f.agent->stats.runs = f.agent->stats.completed_runs = UINT32_MAX;
    f.agent->stats.model_calls = f.agent->stats.iterations = UINT32_MAX;
    assert(run(&f) == AGENT_OK && f.response.stats.completed_runs == UINT32_MAX &&
           f.response.stats.model_calls == UINT32_MAX && f.response.stats.runs == UINT32_MAX);
    setup(&f, FINAL);
    f.config.limits.timeout_ms = 1u;
    f.now = UINT64_MAX;
    assert(run(&f) == AGENT_ERROR_TIMEOUT && f.models == 0u);
    setup(&f, FINAL);
    f.agent->scratch.capacity = 32u;
    assert(run(&f) == AGENT_ERROR_CAPACITY && f.models == 0u && f.begins == 0u);
    setup(&f, FINAL);
    f.agent->model = NULL;
    assert(agent_run(f.agent, &f.request, &f.response) == AGENT_ERROR_NOT_SUPPORTED &&
           f.events == 0u);
    setup(&f, FINAL);
    assert(agent_set_session_storage(f.agent, NULL) == AGENT_OK);
    assert(run(&f) == AGENT_OK && f.appends == 0u && f.finishes == 0u);
#if AGENT_MAX_TOOLS > 0
    setup(&f, TOOL);
    assert(run(&f) == AGENT_OK && f.models == 2u && f.handlers == 1u && f.appends == 4u &&
           f.events == 8u);
    assert(f.sequence[3] == AGENT_EVENT_TOOL_BEGIN && f.sequence[4] == AGENT_EVENT_TOOL_END);
    assert(f.response.summary.tool_succeeded == 1u);
    setup(&f, TOOL);
    add_contributions(&f);
    assert(run(&f) == AGENT_OK && f.reads == 1u && f.contexts == 2u);
    setup(&f, TWO_TOOLS);
    assert(run(&f) == AGENT_OK && f.handlers == 2u && f.appends == 5u);
    setup(&f, UNKNOWN);
    assert(run(&f) == AGENT_OK && f.handlers == 0u && f.response.summary.tool_denied == 1u &&
           f.events == 6u);
    setup(&f, BAD_ARGS);
    assert(run(&f) == AGENT_OK && f.handlers == 0u && f.response.summary.tool_denied == 1u);
    setup(&f, TOOL);
    f.allow = false;
    assert(run(&f) == AGENT_OK && f.handlers == 0u && f.response.summary.tool_denied == 1u);
    setup(&f, TOOL);
    f.handler_error = AGENT_ERROR_IO;
    assert(run(&f) == AGENT_OK && f.response.summary.tool_failed == 1u &&
           f.response.summary.tools_executed);
    setup(&f, TOOL);
    f.tool_timeout = true;
    assert(run(&f) == AGENT_ERROR_TIMEOUT && f.handlers == 1u &&
           f.response.summary.tool_failed == 1u);
    setup(&f, TWO_TOOLS);
    f.config.limits.max_tool_calls = 1u;
    assert(run(&f) == AGENT_ERROR_LIMIT && f.handlers == 0u);
    setup(&f, TOOL);
    f.config.limits.max_tool_calls = 0u;
    assert(run(&f) == AGENT_ERROR_LIMIT && f.handlers == 0u);
    setup(&f, DUPLICATE);
    assert(run(&f) == AGENT_ERROR_MODEL_PARSE && f.handlers == 0u);
    setup(&f, AGAIN);
    f.config.limits.max_steps = 1u;
    assert(run(&f) == AGENT_ERROR_LIMIT && f.handlers == 1u && !f.response.summary.final_valid);
    setup(&f, AGAIN);
    assert(run(&f) == AGENT_ERROR_EXISTS && f.handlers == 1u);
    setup(&f, TOOL);
    f.fail_append = 3u;
    assert(run(&f) == AGENT_ERROR_IO && f.handlers == 1u && f.finishes == 1u);
#endif
    puts("run contract passed");
    return 0;
}
