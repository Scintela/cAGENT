/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 tanglinjie */
#include "core/core_internal.h"
#include "core/text_internal.h"
#include <assert.h>
#include <string.h>

static uint64_t now(void* context) { (void)context; return 1u; }
typedef struct { char text[128]; size_t used; bool fail; } capture_t;
static agent_error_t collect(void* context, agent_string_view_t text)
{
    capture_t* c = context;
    if (c->fail) return AGENT_ERROR_IO;
    assert(text.size <= sizeof(c->text) - c->used);
    memcpy(c->text + c->used, text.data, text.size);
    c->used += text.size;
    return AGENT_OK;
}

int main(void)
{
    static agent_workspace_t workspace;
    agent_t* agent;
    agent_config_t config = agent_config_default();
    agent_skill_t a = {AGENT_SV_LITERAL("a"), {0}, AGENT_SV_LITERAL("AAA"), 10, false};
    capture_t c = {{0}, 0u, false};
    agent_text_sink_t sink = {collect, &c};
    config.runtime.now_ms = now;
    assert(agent_init(&agent, &workspace, &config) == AGENT_OK);
#if AGENT_MAX_SKILLS == 0
    assert(!agent->skills);
    assert(agent_register_skill(agent, &a) == AGENT_ERROR_NOT_SUPPORTED);
#else
    assert(agent_register_skill(agent, &a) == AGENT_OK);
    assert(agent_register_skill(agent, &a) == AGENT_ERROR_EXISTS);
    a.name = agent_string_view("bad/name", 8u);
    assert(agent_register_skill(agent, &a) == AGENT_ERROR_INVALID);
    a.name = agent_string_view("b", 1u);
    a.content = agent_string_view("\xc0\x80", 2u);
    assert(agent_register_skill(agent, &a) == AGENT_ERROR_PARSE);
    a.content = agent_string_view("\xed\xa0\x80", 3u);
    assert(agent_register_skill(agent, &a) == AGENT_ERROR_PARSE);
    a.content = agent_string_view("B", 1u);
#if AGENT_MAX_SKILLS == 1
    assert(agent_register_skill(agent, &a) == AGENT_ERROR_CAPACITY);
#else
    a.priority = -1; a.required = true;
    assert(agent_register_skill(agent, &a) == AGENT_OK);
    assert(agent_skill_project(agent->skills, 1u, &sink) == AGENT_OK);
    assert(c.used == 1u && c.text[0] == 'B');
    c.used = 0u;
    assert(agent_skill_project(agent->skills, 0u, &sink) == AGENT_ERROR_CONTEXT_OVERFLOW);
    assert(c.used == 0u);
    a.name = agent_string_view("c", 1u); a.content = agent_string_view("C", 1u);
    assert(agent_register_skill(agent, &a) == AGENT_OK);
    assert(agent_skill_project(agent->skills, 4u, &sink) == AGENT_OK);
    assert(c.used == 4u && memcmp(c.text, "B\n\nC", 4u) == 0);
    c.used = 0u;
    assert(agent_skill_project(agent->skills, 9u, &sink) == AGENT_OK);
    assert(c.used == 9u && memcmp(c.text, "AAA\n\nB\n\nC", 9u) == 0);
#endif
    agent->in_callback = true;
    assert(agent_unregister_skill(agent, agent_string_view("a", 1u)) == AGENT_ERROR_BUSY);
    agent->in_callback = false;
    agent->state = AGENT_CORE_ACTIVE;
    assert(agent_register_skill(agent, &a) == AGENT_ERROR_BUSY);
    agent->state = AGENT_CORE_READY;
    c.fail = true;
    assert(agent_skill_project(agent->skills, 128u, &sink) == AGENT_ERROR_IO);
    assert(agent_unregister_skill(agent, agent_string_view("a", 1u)) == AGENT_OK);
    assert(agent_unregister_skill(agent, agent_string_view("a", 1u)) == AGENT_ERROR_NOT_FOUND);
#endif
    assert(agent_skill_project(NULL, 0u, &sink) == AGENT_OK);
    assert(agent_text_validate(agent_string_view("\xf0\x9f\x98\x80", 4u)) == AGENT_OK);
    agent_destroy(agent);
    return 0;
}
