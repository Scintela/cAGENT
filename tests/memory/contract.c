/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 tanglinjie */
#include <agent.h>
#include <agent/memory.h>
#include "core/core_internal.h"
#include "memory/memory_internal.h"
#include <assert.h>
#include <string.h>

#define SV(s) agent_string_view((s), sizeof(s) - 1u)
typedef struct {
    agent_t* agent;
    const char* content;
    agent_error_t error;
    agent_memory_change_t change;
    size_t reads, writes, forgets;
    bool reenter, oversized, nul;
} mock_t;

static uint64_t now_ms(void* context) { (void)context; return 1u; }

static agent_error_t read_document(void* context, const agent_memory_key_t* key,
                                   char* output, size_t capacity, size_t max_bytes, size_t* bytes)
{
    mock_t* mock = context;
    size_t length = strlen(mock->content);
    ++mock->reads;
    if (mock->reenter) {
        char other[8];
        agent_string_view_t view;
        agent_memory_change_t change;
        assert(agent_set_memory(mock->agent, NULL) == AGENT_ERROR_BUSY);
        assert(agent_memory_read(mock->agent, key, other, sizeof(other), 7u, &view) == AGENT_ERROR_BUSY);
        assert(agent_memory_replace(mock->agent, key, SV("new"), &change) == AGENT_ERROR_BUSY);
        agent_destroy(mock->agent);
        assert(mock->agent->has_memory && mock->agent->in_callback);
    }
    if (mock->error != AGENT_OK) return mock->error;
    if (length >= capacity || length > max_bytes) return AGENT_ERROR_CAPACITY;
    memcpy(output, mock->content, length);
    if (mock->nul && length) output[0] = '\0';
    *bytes = mock->oversized ? capacity : length;
    return AGENT_OK;
}

static agent_error_t replace_document(void* context, const agent_memory_key_t* key,
                                      agent_string_view_t text, agent_memory_change_t* change)
{
    mock_t* mock = context;
    (void)key; (void)text;
    ++mock->writes;
    *change = mock->change;
    return mock->error;
}

static agent_error_t forget_document(void* context, const agent_memory_key_t* key,
                                     agent_memory_change_t* change)
{
    mock_t* mock = context;
    (void)key;
    ++mock->forgets;
    *change = mock->change;
    return mock->error;
}

int main(void)
{
    static agent_workspace_t workspace;
    agent_config_t config = agent_config_default();
    agent_t* agent;
    mock_t mock = {0};
    agent_memory_t memory = {{read_document, replace_document, forget_document}, &mock};
    agent_memory_t invalid = {{NULL, NULL, NULL}, NULL};
    agent_memory_key_t user = {AGENT_MEMORY_USER, {NULL, 0u}};
    agent_memory_key_t soul = {AGENT_MEMORY_SOUL, {NULL, 0u}};
    agent_memory_key_t note = {AGENT_MEMORY_NOTE, {NULL, 0u}};
    agent_string_view_t view;
    agent_memory_change_t change;
    char output[16];
    size_t reads, mark;
    config.runtime.now_ms = now_ms;
    assert(agent_init(&agent, &workspace, &config) == AGENT_OK);
    mock.agent = agent; mock.content = "hello"; mock.change = AGENT_MEMORY_APPLIED;
    assert(agent_memory_read(agent, &user, output, sizeof(output), 8u, &view) == AGENT_ERROR_NOT_SUPPORTED);
    assert(!view.data && output[0] == '\0');
    assert(agent_set_memory(NULL, &memory) == AGENT_ERROR_INVALID);
    assert(agent_set_memory(agent, &invalid) == AGENT_ERROR_INVALID);
    assert(agent_set_memory(agent, &memory) == AGENT_OK);
    memory.ops.read = NULL; /* Binding is copied, not retained. */
    assert(agent_start(agent) == AGENT_OK);
    assert(agent_memory_read(agent, &user, output, sizeof(output), 8u, &view) == AGENT_OK);
    assert(view.data == output && view.size == 5u && !strcmp(output, "hello"));
    assert(agent_memory_read(agent, &user, output, 5u, 8u, &view) == AGENT_ERROR_CAPACITY);
    assert(!view.data && output[0] == '\0');
    assert(agent_memory_read(agent, &user, output, sizeof(output), 4u, &view) == AGENT_ERROR_CAPACITY);
    assert(agent_memory_read(agent, &user, output, sizeof(output), 0u, &view) == AGENT_ERROR_INVALID);
    assert(agent_memory_read(agent, &note, output, sizeof(output), 8u, &view) == AGENT_ERROR_INVALID);
    note.id = SV("entry");
    assert(agent_memory_read(agent, &note, output, sizeof(output), 8u, &view) == AGENT_OK);
    user.id = SV("unexpected");
    assert(agent_memory_read(agent, &user, output, sizeof(output), 8u, &view) == AGENT_ERROR_INVALID);
    user.id = agent_string_view(NULL, 0u);
    assert(agent_memory_read(agent, &user, (char*)&workspace, sizeof(output), 8u, &view) == AGENT_ERROR_INVALID);
    assert(agent_memory_read(agent, &user, output, sizeof(output), 8u, (agent_string_view_t*)output) == AGENT_ERROR_INVALID);
    mock.oversized = true;
    assert(agent_memory_read(agent, &user, output, sizeof(output), 8u, &view) == AGENT_ERROR_IO);
    assert(!view.data && output[0] == '\0');
    mock.oversized = false; mock.nul = true;
    assert(agent_memory_read(agent, &user, output, sizeof(output), 8u, &view) == AGENT_ERROR_PARSE);
    mock.nul = false; mock.reenter = true;
    assert(agent_memory_read(agent, &user, output, sizeof(output), 8u, &view) == AGENT_OK);
    mock.reenter = false;
    mock.error = AGENT_ERROR_NOT_FOUND;
    assert(agent_memory_read(agent, &user, output, sizeof(output), 8u, &view) == AGENT_ERROR_NOT_FOUND);
    mock.error = AGENT_OK;
    reads = mock.reads;
    assert(agent_memory_replace(agent, &soul, SV("overwrite"), &change) == AGENT_ERROR_POLICY_DENIED);
    assert(change == AGENT_MEMORY_UNCHANGED && mock.writes == 0u);
    assert(agent_memory_forget(agent, &soul, &change) == AGENT_ERROR_POLICY_DENIED && mock.forgets == 0u);
    assert(agent_memory_replace(agent, &user, SV("a\0b"), &change) == AGENT_ERROR_PARSE);
    assert(agent_memory_replace(agent, &user, SV("new"), &change) == AGENT_OK && change == AGENT_MEMORY_APPLIED);
    assert(agent_memory_forget(agent, &user, &change) == AGENT_OK && change == AGENT_MEMORY_APPLIED);
    mock.error = AGENT_ERROR_IO;
    assert(agent_memory_replace(agent, &user, SV("new"), &change) == AGENT_ERROR_IO && change == AGENT_MEMORY_APPLIED);
    mock.change = AGENT_MEMORY_UNKNOWN;
    assert(agent_memory_forget(agent, &user, &change) == AGENT_ERROR_IO && change == AGENT_MEMORY_UNKNOWN);
    mock.error = AGENT_OK; mock.change = AGENT_MEMORY_UNCHANGED;
    assert(agent_memory_replace(agent, &user, SV("new"), &change) == AGENT_ERROR_IO && change == AGENT_MEMORY_UNKNOWN);
    mock.change = (agent_memory_change_t)42;
    assert(agent_memory_forget(agent, &user, &change) == AGENT_ERROR_IO && change == AGENT_MEMORY_UNKNOWN);
    assert(mock.reads == reads);

    agent->state = AGENT_CORE_ACTIVE;
    assert(agent_memory_replace(agent, &user, SV("new"), &change) == AGENT_ERROR_BUSY);
    assert(agent_memory_read(agent, &user, output, sizeof(output), 8u, &view) == AGENT_ERROR_BUSY);
    mark = agent->scratch.used;
    assert(agent_memory_project(agent, &user, &agent->scratch, 8u,
                                (agent_string_view_t*)&agent->scratch) == AGENT_ERROR_INVALID);
    assert(agent->scratch.used == mark);
    assert(agent_memory_project(agent, &user, &agent->scratch, 8u, &view) == AGENT_OK);
    assert(view.size == 5u && !memcmp(view.data, "hello", 5u));
    assert(agent->scratch.used == mark + 6u);
    mock.content = "updated";
    assert(!memcmp(view.data, "hello", 5u));
    mark = agent->scratch.used;
    mock.error = AGENT_ERROR_IO;
    assert(agent_memory_project(agent, &user, &agent->scratch, 8u, &view) == AGENT_ERROR_IO);
    assert(!view.data && agent->scratch.used == mark);
    mock.error = AGENT_OK;
    assert(agent_memory_project(agent, &user, &agent->scratch, SIZE_MAX, &view) == AGENT_ERROR_LIMIT);
    assert(agent_memory_project(agent, &user, &agent->scratch, AGENT_SCRATCH_BYTES, &view) == AGENT_ERROR_CAPACITY);
    assert(agent->scratch.used == mark);
    agent->state = AGENT_CORE_READY;
    memory.ops.read = read_document; memory.ops.replace = NULL; memory.ops.forget = NULL;
    assert(agent_set_memory(agent, &memory) == AGENT_OK);
    assert(agent_memory_replace(agent, &user, SV("new"), &change) == AGENT_ERROR_NOT_SUPPORTED);
    assert(change == AGENT_MEMORY_UNCHANGED);
    assert(agent_set_memory(agent, NULL) == AGENT_OK);
    assert(agent_memory_forget(agent, &user, &change) == AGENT_ERROR_NOT_SUPPORTED);
    assert(agent_memory_project(agent, &user, &agent->scratch, 8u, &view) == AGENT_ERROR_NOT_SUPPORTED);
    agent_destroy(agent);
    return 0;
}
