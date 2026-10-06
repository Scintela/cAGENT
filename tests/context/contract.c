/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 tanglinjie */
#include "core/core_internal.h"
#include <agent_session_ram.h>
#include <assert.h>
#include <string.h>

#define SV(s) agent_string_view((s), sizeof(s) - 1u)
static uint64_t now;
static uint64_t clock_ms(void* context) { (void)context; return now; }
static agent_t* agent;
static agent_cancel_token_t cancel;
static size_t calls, reads;
static unsigned mode;
static char saved_memory[32] = "preference";

static agent_error_t build(void* data, const agent_context_request_t* request,
                            const agent_text_sink_t* sink)
{
    agent_error_t status;
    (void)data;
    ++calls;
    assert(request->request->limits && request->request->user_data == &calls);
    assert(agent_register_context(agent, NULL) == AGENT_ERROR_BUSY);
    assert(agent_unregister_skill(agent, SV("skill")) == AGENT_ERROR_BUSY);
    assert(agent_set_memory(agent, NULL) == AGENT_ERROR_BUSY);
    if (mode == 1u) { sink->write(sink->context, SV("partial")); return AGENT_ERROR_IO; }
    if (mode == 2u) { now = 100u; return AGENT_OK; }
    if (mode == 3u) { agent_cancel_token_request_locked(&cancel); return AGENT_OK; }
    if (mode == 4u) {
        static char large[129];
        memset(large, 'x', sizeof(large));
        assert(sink->write(sink->context, agent_string_view(large, sizeof(large))) == AGENT_ERROR_CONTEXT_OVERFLOW);
        assert(sink->write(sink->context, SV("ignored")) == AGENT_ERROR_CONTEXT_OVERFLOW);
        return AGENT_OK;
    }
    if (mode == 5u) return sink->write(sink->context, SV("\xed\xa0\x80"));
    status = sink->write(sink->context, SV("\xe4"));
    if (status != AGENT_OK) return status;
    return sink->write(sink->context, SV("\xb8\xad"));
}

static agent_error_t read_memory(void* context, const agent_memory_key_t* key,
    char* output, size_t capacity, size_t maximum, size_t* bytes)
{
    size_t n = strlen(saved_memory);
    (void)context; (void)key;
    ++reads;
    assert(agent_set_memory(agent, NULL) == AGENT_ERROR_BUSY);
    if (mode == 6u) return AGENT_ERROR_NOT_FOUND;
    assert(n <= maximum && n < capacity);
    memcpy(output, saved_memory, n); *bytes = n;
    return AGENT_OK;
}

static agent_error_t execute(void* data, const agent_tool_context_t* context, const agent_text_sink_t* sink)
{ (void)data; (void)context; (void)sink; return AGENT_OK; }

static void reset(void)
{
    static agent_workspace_t workspace;
    agent_config_t config = agent_config_default();
    config.runtime.now_ms = clock_ms;
    config.system_prompt = SV("system");
    assert(agent_init(&agent, &workspace, &config) == AGENT_OK);
    agent_cancel_token_init(&cancel, NULL);
    now = 1u; calls = 0u; reads = 0u; mode = 0u;
    strcpy(saved_memory, "preference");
}

static agent_request_t request(void)
{
    agent_request_t r = {0};
    r.input = SV("question"); r.session_id = SV("home"); r.trace_id = SV("trace");
    r.user_data = &calls;
    return r;
}

static agent_session_turn_t* open_session(void)
{
    agent_session_turn_t* session;
    assert(agent_start(agent) == AGENT_OK);
    agent->state = AGENT_CORE_ACTIVE;
    assert(agent_session_turn_open(&session, &agent->scratch, NULL, SV("home"), SV("question")) == AGENT_OK);
    return session;
}

static void test_projection(void)
{
    agent_context_provider_t p = {0};
    agent_memory_context_t m = {0};
    agent_memory_t memory = {{read_memory, NULL, NULL}, NULL};
    agent_skill_t skill = {0};
    agent_tool_t tool = {0};
    agent_context_turn_t* turn;
    agent_context_projection_t projection;
    agent_session_turn_t* session;
    agent_request_t r = request();
    size_t mark, i;
    reset();
    p.name = SV("state"); p.build = build; p.max_bytes = 128u; p.placement = AGENT_CONTEXT_REFERENCE;
    skill.name = SV("skill"); skill.content = SV("instructions"); skill.required = true;
    tool.name = SV("lamp"); tool.input_schema_json = SV("{}"); tool.execute = execute;
#if AGENT_MAX_SKILLS > 0
    assert(agent_register_skill(agent, &skill) == AGENT_OK);
#else
    assert(agent_register_skill(agent, &skill) == AGENT_ERROR_NOT_SUPPORTED);
#endif
#if AGENT_MAX_TOOLS > 0
    assert(agent_register_tool(agent, &tool) == AGENT_OK);
#else
    assert(agent_register_tool(agent, &tool) == AGENT_ERROR_NOT_SUPPORTED);
#endif
#if AGENT_MAX_CONTEXTS > 0
    assert(agent_register_context(agent, &p) == AGENT_OK);
    assert(agent_register_context(agent, &p) == AGENT_ERROR_EXISTS);
#else
    assert(agent_register_context(agent, &p) == AGENT_ERROR_NOT_SUPPORTED);
#endif
    assert(agent_set_memory(agent, &memory) == AGENT_OK);
    m.name = SV("facts"); m.key.kind = AGENT_MEMORY_FACTS; m.max_bytes = 32u; m.required = true;
#if AGENT_MAX_CONTEXTS > 1
    assert(agent_register_memory_context(agent, &m) == AGENT_OK);
#elif AGENT_MAX_CONTEXTS == 1
    assert(agent_register_memory_context(agent, &m) == AGENT_ERROR_CAPACITY);
#else
    assert(agent_register_memory_context(agent, &m) == AGENT_ERROR_NOT_SUPPORTED);
#endif
    session = open_session();
    assert(agent_context_prepare(agent, &r, &cancel, 100u, &turn) == AGENT_OK);
    mark = agent->scratch.used;
    strcpy(saved_memory, "changed");
    for (i = 0u; i < 4u; ++i) {
        assert(agent_context_project(turn, session, 1u, 0u, &projection) == AGENT_OK);
        assert(projection.request.max_output_tokens == agent->config.limits.max_output_tokens);
        assert(projection.request.timeout_ms == 99u);
        assert(projection.request.messages[projection.request.message_count - 1u].content.size == 8u);
#if AGENT_MAX_TOOLS > 0
        assert(projection.request.tool_count == 1u);
#else
        assert(projection.request.tool_count == 0u);
#endif
#if AGENT_MAX_SKILLS > 0
        assert(strcmp(projection.request.system_prompt.data, "system\n\ninstructions") == 0);
#endif
#if AGENT_MAX_CONTEXTS > 1
        assert(reads == 1u);
        assert(projection.request.messages[0].content.size == 10u);
        assert(memcmp(projection.request.messages[0].content.data, "preference", 10u) == 0);
#endif
        assert(agent_context_release(&projection) == AGENT_OK);
        assert(agent->scratch.used == mark);
    }
#if AGENT_MAX_CONTEXTS > 0
    assert(calls == 4u);
    for (mode = 1u; mode <= 5u; ++mode) {
        agent_error_t expected = mode == 2u ? AGENT_ERROR_TIMEOUT : mode == 3u ? AGENT_ERROR_CANCELLED : AGENT_OK;
        now = 1u; agent_cancel_token_init(&cancel, NULL);
        assert(agent_context_project(turn, session, 0u, 0u, &projection) == expected);
        assert(!agent->in_callback);
        if (expected == AGENT_OK) {
            assert(projection.request.tool_count == 0u);
            assert(projection.report.count == 1u);
            assert(projection.report.entries[0].status == (mode == 1u ? AGENT_ERROR_IO :
                    mode == 4u ? AGENT_ERROR_CONTEXT_OVERFLOW : AGENT_ERROR_PARSE));
            assert(agent_context_release(&projection) == AGENT_OK);
        }
        assert(agent->scratch.used == mark);
    }
#endif
    mode = 0u; now = 1u; agent_cancel_token_init(&cancel, NULL);
    assert(agent_context_project(turn, session, 1u, agent->scratch.capacity, &projection) == AGENT_ERROR_CAPACITY);
    assert(agent->scratch.used == mark);
    agent->state = AGENT_CORE_READY;
    agent_destroy(agent);
}

static void test_failure(void)
{
#if AGENT_MAX_CONTEXTS > 0
    agent_context_provider_t p = {0};
    agent_context_turn_t* turn = NULL;
    agent_context_projection_t projection;
    agent_session_turn_t* session;
    agent_request_t r = request();
    size_t mark;
    reset();
    p.name = SV("required"); p.required = true; p.build = build; p.max_bytes = 128u;
    assert(agent_register_context(agent, &p) == AGENT_OK);
    session = open_session();
    assert(agent_context_prepare(agent, &r, &cancel, 100u, &turn) == AGENT_OK);
    mark = agent->scratch.used;
    mode = 1u;
    assert(agent_context_project(turn, session, 0u, 0u, &projection) == AGENT_ERROR_IO);
    assert(agent->scratch.used == mark && projection.owner == NULL);
    mode = 0u;
    assert(agent_context_project(turn, session, 0u, 0u, &projection) == AGENT_OK);
    assert(agent_context_release(&projection) == AGENT_OK);
    agent->state = AGENT_CORE_READY;
    agent_destroy(agent);
    reset();
    p.max_bytes = AGENT_MAX_CONTEXT_BYTES;
    assert(agent_register_context(agent, &p) == AGENT_OK);
    (void)open_session(); mark = agent->scratch.used;
    assert(agent_context_prepare(agent, &r, &cancel, 0u, &turn) == AGENT_ERROR_CONTEXT_OVERFLOW);
    assert(!turn && agent->scratch.used == mark);
    agent->state = AGENT_CORE_READY;
    agent_destroy(agent);
#endif
}

static void test_history(void)
{
    static agent_session_ram_turn_t turns[4];
    static agent_session_ram_message_t stored[16];
    static agent_session_ram_call_t calls_storage[4];
    static agent_message_view_t views[16];
    static agent_tool_call_view_t call_views[4];
    static char payload[2048];
    agent_session_ram_config_t config = {turns, 4u, stored, 16u, calls_storage, 4u,
        payload, sizeof(payload), views, 16u, call_views, 4u};
    agent_session_ram_t ram;
    agent_session_storage_t storage;
    agent_session_turn_t* session;
    agent_context_turn_t* turn;
    agent_context_projection_t projection;
    agent_message_view_t final = {0};
    agent_request_t r = request();
    size_t mark, groups;
    reset();
    assert(agent_session_ram_init(&ram, &config) == AGENT_OK);
    assert(agent_session_ram_bind(&ram, &storage) == AGENT_OK);
    assert(agent_set_session_storage(agent, &storage) == AGENT_OK);
    agent->config.limits.max_history_turns = 2u;
    assert(agent_start(agent) == AGENT_OK);
    agent->state = AGENT_CORE_ACTIVE;
    assert(agent_session_turn_open(&session, &agent->scratch, &storage, SV("home"), SV("old")) == AGENT_OK);
    final.role = AGENT_MESSAGE_ROLE_ASSISTANT; final.content = SV("answer");
    assert(agent_session_append(session, &final) == AGENT_OK);
    assert(agent_session_turn_finish(session, AGENT_SESSION_TURN_COMPLETE) == AGENT_OK);
    agent_arena_rewind(&agent->scratch, 0u);
    assert(agent_session_turn_open(&session, &agent->scratch, &storage, SV("home"), r.input) == AGENT_OK);
    assert(agent_context_prepare(agent, &r, &cancel, 100u, &turn) == AGENT_OK);
    mark = agent->scratch.used;
    assert(agent_context_project(turn, session, 0u, 0u, &projection) == AGENT_OK);
    assert(projection.request.message_count == 3u);
    assert(memcmp(projection.request.messages[0].content.data, "old", 3u) == 0);
    assert(projection.request.messages[1].role == AGENT_MESSAGE_ROLE_ASSISTANT);
    assert(agent_context_release(&projection) == AGENT_OK && agent->scratch.used == mark);
    /* Response facts are appended only after disposable input views are released. */
    assert(agent_session_append(session, &final) == AGENT_OK);
    assert(agent_session_turn_finish(session, AGENT_SESSION_TURN_COMPLETE) == AGENT_OK);
    agent->state = AGENT_CORE_READY;
    assert(agent_session_count(agent, &groups) == AGENT_OK && groups == 1u);
    assert(ram.turn_count == 2u && ram.message_count == 4u);
    agent_destroy(agent);
}

static void test_memory_admission(void)
{
#if AGENT_MAX_CONTEXTS > 0
    agent_memory_t memory = {{read_memory, NULL, NULL}, NULL};
    agent_memory_context_t m = {0};
    agent_context_turn_t* turn;
    agent_context_projection_t projection;
    agent_session_turn_t* session;
    agent_request_t r = request();
    size_t mark;
    reset();
    assert(agent_set_memory(agent, &memory) == AGENT_OK);
    m.name = SV("soul"); m.key.kind = AGENT_MEMORY_SOUL; m.max_bytes = 32u;
    assert(agent_register_memory_context(agent, &m) == AGENT_OK);
    session = open_session(); mark = agent->scratch.used;
    mode = 6u;
    assert(agent_context_prepare(agent, &r, &cancel, 100u, &turn) == AGENT_OK);
    assert(turn->report.count == 1u && turn->report.entries[0].status == AGENT_ERROR_NOT_FOUND);
    assert(agent_context_project(turn, session, 0u, 0u, &projection) == AGENT_OK);
    assert(strcmp(projection.request.system_prompt.data, "system") == 0);
    assert(agent_context_release(&projection) == AGENT_OK);
    agent_arena_rewind(&agent->scratch, mark);
    agent->state = AGENT_CORE_READY;
    assert(agent_unregister_context(agent, m.name) == AGENT_OK);
    m.required = true;
    assert(agent_register_memory_context(agent, &m) == AGENT_OK);
    agent->state = AGENT_CORE_ACTIVE;
    assert(agent_context_prepare(agent, &r, &cancel, 100u, &turn) == AGENT_ERROR_NOT_FOUND);
    assert(!turn && agent->scratch.used == mark);
    mode = 0u;
    assert(agent_context_prepare(agent, &r, &cancel, 100u, &turn) == AGENT_OK);
    assert(agent_context_project(turn, session, 0u, 0u, &projection) == AGENT_OK);
    assert(strcmp(projection.request.system_prompt.data, "system\n\npreference") == 0);
    assert(projection.request.message_count == 1u);
    assert(agent_context_release(&projection) == AGENT_OK);
    agent->state = AGENT_CORE_READY;
    agent_destroy(agent);
#endif
}

static agent_error_t literal(void* data, const agent_context_request_t* request,
                              const agent_text_sink_t* sink)
{
    const char* text = data;
    (void)request;
    return sink->write(sink->context, agent_string_view(text, strlen(text)));
}

static void test_order_and_boundaries(void)
{
    agent_context_provider_t p = {0};
    agent_context_turn_t* turn;
    agent_context_projection_t projection;
    agent_session_turn_t* session;
    agent_request_t r = request();
    reset();
    p.name = SV("a"); p.build = literal; p.user_data = (void*)"A"; p.max_bytes = 1u;
    p.priority = 10;
#if AGENT_MAX_CONTEXTS > 2
    assert(agent_register_context(agent, &p) == AGENT_OK);
    p.name = SV("b"); p.user_data = (void*)"B";
    assert(agent_register_context(agent, &p) == AGENT_OK);
    p.name = SV("c"); p.user_data = (void*)"C"; p.required = true; p.priority = -1;
    assert(agent_register_context(agent, &p) == AGENT_OK);
    session = open_session();
    assert(agent_context_prepare(agent, &r, &cancel, 0u, &turn) == AGENT_OK);
    assert(agent_context_project(turn, session, 0u, 0u, &projection) == AGENT_OK);
    assert(strcmp(projection.request.system_prompt.data, "system\n\nA\n\nB\n\nC") == 0);
    assert(agent_context_release(&projection) == AGENT_OK);
    agent->state = AGENT_CORE_READY;
    agent_destroy(agent);
    reset();
#endif
    agent->config.system_prompt = SV("");
    p.name = SV("default"); p.required = true; p.max_bytes = 0u;
#if AGENT_MAX_CONTEXTS > 0
    assert(agent_register_context(agent, &p) == AGENT_OK);
#else
    assert(agent_register_context(agent, &p) == AGENT_ERROR_NOT_SUPPORTED);
#endif
    session = open_session();
    assert(agent_context_prepare(agent, &r, &cancel, 0u, &turn) == AGENT_OK);
    assert(agent_context_project(turn, session, 0u, 0u, &projection) == AGENT_OK);
    assert(agent_context_release(&projection) == AGENT_OK);
    agent->state = AGENT_CORE_READY;
    agent_destroy(agent);
}

int main(void)
{
    test_projection();
    test_failure();
    test_history();
    test_memory_admission();
    test_order_and_boundaries();
    return 0;
}
