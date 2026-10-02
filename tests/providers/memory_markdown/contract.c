/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 tanglinjie */
#define _POSIX_C_SOURCE 200809L
#include <agent.h>
#include <agent_markdown_memory.h>
#include <agent_posix_file_store.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SV(s) agent_string_view((s), sizeof(s) - 1u)
typedef struct {
    agent_file_store_t store;
    int replace_failure, remove_failure;
    agent_markdown_memory_t* memory;
    bool reenter;
} proxy_t;

static uint64_t now_ms(void* context) { (void)context; return 1u; }
static agent_error_t size_file(void* context, agent_string_view_t name, uint64_t* bytes)
{
    proxy_t* proxy = context;
    return agent_file_size(&proxy->store, name, bytes);
}
static agent_error_t read_file(void* context, agent_string_view_t name, uint64_t offset,
                               void* output, size_t capacity, size_t* bytes)
{
    proxy_t* proxy = context;
    if (proxy->reenter) {
        agent_memory_t binding;
        assert(agent_markdown_memory_bind(proxy->memory, &binding) == AGENT_ERROR_BUSY);
    }
    return agent_file_read(&proxy->store, name, offset, output, capacity, bytes);
}
static agent_error_t replace_file(void* context, agent_string_view_t name,
                                  const void* text, size_t bytes, bool* published)
{
    proxy_t* proxy = context;
    agent_error_t status;
    if (proxy->replace_failure == 1) return AGENT_ERROR_IO;
    status = agent_file_replace(&proxy->store, name, text, bytes, published);
    return status == AGENT_OK && proxy->replace_failure == 2 ? AGENT_ERROR_IO : status;
}
static agent_error_t remove_file(void* context, agent_string_view_t name)
{
    proxy_t* proxy = context;
    agent_error_t status;
    if (proxy->remove_failure == 1) return AGENT_ERROR_IO;
    status = agent_file_remove(&proxy->store, name);
    return status == AGENT_OK && proxy->remove_failure == 2 ? AGENT_ERROR_IO : status;
}

static void expect_document(agent_t* agent, const agent_memory_key_t* key, const char* expected)
{
    char output[64];
    agent_string_view_t view;
    assert(agent_memory_read(agent, key, output, sizeof(output), 63u, &view) == AGENT_OK);
    assert(view.data == output && view.size == strlen(expected) && !strcmp(output, expected));
}

int main(void)
{
    char root[] = "/tmp/cagent-memory-XXXXXX", notes[] = "/tmp/cagent-notes-XXXXXX";
    char other[] = "/tmp/cagent-memory-other-XXXXXX";
    char paths[512], note_paths[512], other_paths[512], snapshot[64], output[64];
    agent_posix_file_store_t file_state, note_state, other_state;
    agent_file_store_t store, note_store, other_store;
    agent_posix_file_store_config_t file_config = {root, paths, sizeof(paths), false, true};
    agent_markdown_memory_config_t config = {0}, invalid;
    agent_markdown_memory_t memory, second, readonly;
    agent_memory_t binding, other_binding, readonly_binding;
    agent_memory_key_t soul = {AGENT_MEMORY_SOUL, {NULL, 0u}};
    agent_memory_key_t user = {AGENT_MEMORY_USER, {NULL, 0u}};
    agent_memory_key_t facts = {AGENT_MEMORY_FACTS, {NULL, 0u}};
    agent_memory_key_t note = {AGENT_MEMORY_NOTE, AGENT_SV_LITERAL("2026-10-02")};
    static agent_workspace_t workspace, second_workspace;
    agent_config_t agent_config = agent_config_default();
    agent_t* agent; agent_t* second_agent;
    agent_memory_change_t change;
    agent_string_view_t view;
    uint64_t bytes;
    size_t count;
    const agent_file_store_ops_t proxy_ops = {size_file, read_file, NULL, NULL, NULL, NULL,
                                             remove_file, replace_file};
    agent_file_store_ops_t readonly_ops = proxy_ops;
    proxy_t proxy = {0};
    agent_file_store_t proxy_store = {&proxy_ops, &proxy};
    const char* bad_dates[] = {"2026-02-29", "1900-02-29", "0000-01-01", "2026-13-01",
                               "2026-00-01", "2026-04-31", "2026-10-00", "../USER.md",
                               "2026-10-02.md", "2026-aa-01"};
    size_t i;

    assert(mkdtemp(root) && mkdtemp(notes) && mkdtemp(other));
    assert(agent_posix_file_store_init(&file_state, &file_config, &store) == AGENT_OK);
    file_config.directory = notes; file_config.path_buffer = note_paths;
    assert(agent_posix_file_store_init(&note_state, &file_config, &note_store) == AGENT_OK);
    file_config.directory = other; file_config.path_buffer = other_paths;
    assert(agent_posix_file_store_init(&other_state, &file_config, &other_store) == AGENT_OK);
    proxy.store = store; proxy.memory = &memory;
    config.soul = (agent_markdown_memory_document_t){proxy_store, SV("SOUL.md"), 32u};
    config.user = (agent_markdown_memory_document_t){proxy_store, SV("USER.md"), 32u};
    config.facts = (agent_markdown_memory_document_t){proxy_store, SV("MEMORY.md"), 32u};
    config.notes = note_store; config.note_max_bytes = 32u;
    assert(agent_markdown_memory_init(NULL, &config) == AGENT_ERROR_INVALID);
    assert(agent_markdown_memory_init(&memory, NULL) == AGENT_ERROR_INVALID);
    invalid = config; invalid.user.name = SV("SOUL.md");
    assert(agent_markdown_memory_init(&memory, &invalid) == AGENT_ERROR_INVALID);
    invalid = config; invalid.notes = proxy_store;
    assert(agent_markdown_memory_init(&memory, &invalid) == AGENT_ERROR_INVALID);
    invalid = config; invalid.user.name = SV("../USER.md");
    assert(agent_markdown_memory_init(&memory, &invalid) == AGENT_ERROR_INVALID);
    invalid = config; invalid.user.store.ops = NULL;
    assert(agent_markdown_memory_init(&memory, &invalid) == AGENT_ERROR_NOT_SUPPORTED);
    assert(agent_markdown_memory_init(&memory, &config) == AGENT_OK);
    assert(agent_markdown_memory_bind(&memory, &binding) == AGENT_OK);
    agent_config.runtime.now_ms = now_ms;
    assert(agent_init(&agent, &workspace, &agent_config) == AGENT_OK);
    assert(agent_set_memory(agent, &binding) == AGENT_OK && agent_start(agent) == AGENT_OK);
    assert(agent_memory_read(agent, &soul, output, sizeof(output), 32u, &view) == AGENT_ERROR_NOT_FOUND);
    assert(!view.data && output[0] == '\0');
    assert(agent_file_append(&store, SV("SOUL.md"), "trusted identity", 16u) == AGENT_OK);
    assert(agent_file_append(&store, SV("session-61.jsonl"), "original", 8u) == AGENT_OK);
    expect_document(agent, &soul, "trusted identity");
    assert(agent_memory_replace(agent, &soul, SV("attack"), &change) == AGENT_ERROR_POLICY_DENIED);
    assert(binding.ops.replace(binding.context, &soul, SV("attack"), &change) == AGENT_ERROR_POLICY_DENIED);
    assert(binding.ops.forget(binding.context, &soul, &change) == AGENT_ERROR_POLICY_DENIED);
    expect_document(agent, &soul, "trusted identity");
    assert(agent_memory_replace(agent, &user, SV("old profile"), &change) == AGENT_OK);
    assert(change == AGENT_MEMORY_APPLIED);
    assert(agent_memory_read(agent, &user, snapshot, sizeof(snapshot), 32u, &view) == AGENT_OK);
    assert(agent_memory_replace(agent, &user, SV("new profile"), &change) == AGENT_OK);
    assert(!strcmp(snapshot, "old profile"));
    expect_document(agent, &user, "new profile");
    proxy.reenter = true;
    expect_document(agent, &user, "new profile");
    proxy.reenter = false;
    assert(agent_memory_read(agent, &user, output, 4u, 32u, &view) == AGENT_ERROR_CAPACITY);
    assert(agent_memory_read(agent, &user, output, sizeof(output), 4u, &view) == AGENT_ERROR_CAPACITY);
    assert(agent_memory_replace(agent, &user, SV("012345678901234567890123456789012"), &change) == AGENT_ERROR_CAPACITY);
    expect_document(agent, &user, "new profile");
    assert(agent_memory_replace(agent, &user, SV("a\0b"), &change) == AGENT_ERROR_PARSE);
    proxy.replace_failure = 1;
    assert(agent_memory_replace(agent, &user, SV("not published"), &change) == AGENT_ERROR_IO);
    assert(change == AGENT_MEMORY_UNCHANGED);
    proxy.replace_failure = 0;
    expect_document(agent, &user, "new profile");
    proxy.replace_failure = 2;
    assert(agent_memory_replace(agent, &user, SV("published"), &change) == AGENT_ERROR_IO);
    assert(change == AGENT_MEMORY_APPLIED);
    proxy.replace_failure = 0;
    expect_document(agent, &user, "published");
    proxy.remove_failure = 1;
    assert(agent_memory_forget(agent, &user, &change) == AGENT_ERROR_IO && change == AGENT_MEMORY_UNKNOWN);
    proxy.remove_failure = 0;
    expect_document(agent, &user, "published");
    proxy.remove_failure = 2;
    assert(agent_memory_forget(agent, &user, &change) == AGENT_ERROR_IO && change == AGENT_MEMORY_UNKNOWN);
    proxy.remove_failure = 0;
    assert(agent_memory_read(agent, &user, output, sizeof(output), 32u, &view) == AGENT_ERROR_NOT_FOUND);
    assert(agent_memory_forget(agent, &user, &change) == AGENT_ERROR_NOT_FOUND && change == AGENT_MEMORY_UNCHANGED);
    assert(agent_memory_replace(agent, &facts, SV("verified fact"), &change) == AGENT_OK);
    expect_document(agent, &facts, "verified fact");
    assert(agent_memory_replace(agent, &note, SV("daily candidate"), &change) == AGENT_OK);
    expect_document(agent, &note, "daily candidate");
    for (i = 0u; i < sizeof(bad_dates) / sizeof(bad_dates[0]); ++i) {
        agent_memory_key_t bad = {AGENT_MEMORY_NOTE, {bad_dates[i], strlen(bad_dates[i])}};
        assert(agent_memory_replace(agent, &bad, SV("bad"), &change) == AGENT_ERROR_INVALID);
    }
    {
        agent_memory_key_t leap = {AGENT_MEMORY_NOTE, AGENT_SV_LITERAL("2000-02-29")};
        assert(agent_memory_replace(agent, &leap, SV("leap"), &change) == AGENT_OK);
        assert(agent_memory_forget(agent, &leap, &change) == AGENT_OK);
    }
    assert(agent_file_read_exact(&store, SV("session-61.jsonl"), 0u, output, 8u) == AGENT_OK);
    assert(!memcmp(output, "original", 8u));

    invalid = config;
    invalid.soul.max_bytes = 0u; invalid.facts.max_bytes = 0u; invalid.note_max_bytes = 0u;
    invalid.user.store = other_store;
    assert(agent_markdown_memory_init(&second, &invalid) == AGENT_OK);
    assert(agent_markdown_memory_bind(&second, &other_binding) == AGENT_OK);
    assert(agent_init(&second_agent, &second_workspace, &agent_config) == AGENT_OK);
    assert(agent_set_memory(second_agent, &other_binding) == AGENT_OK);
    assert(agent_memory_replace(second_agent, &user, SV("other user"), &change) == AGENT_OK);
    expect_document(second_agent, &user, "other user");
    assert(agent_memory_read(agent, &user, output, sizeof(output), 32u, &view) == AGENT_ERROR_NOT_FOUND);
    assert(agent_memory_read(second_agent, &soul, output, sizeof(output), 32u, &view) == AGENT_ERROR_NOT_SUPPORTED);
    assert(agent_memory_read(second_agent, &note, output, sizeof(output), 32u, &view) == AGENT_ERROR_NOT_SUPPORTED);

    readonly_ops.replace = NULL; readonly_ops.remove = NULL;
    invalid.user.store = (agent_file_store_t){&readonly_ops, &proxy};
    assert(agent_markdown_memory_init(&readonly, &invalid) == AGENT_OK);
    assert(agent_markdown_memory_bind(&readonly, &readonly_binding) == AGENT_OK);
    assert(agent_set_memory(second_agent, &readonly_binding) == AGENT_OK);
    assert(agent_memory_replace(second_agent, &user, SV("write"), &change) == AGENT_ERROR_NOT_SUPPORTED);
    assert(change == AGENT_MEMORY_UNCHANGED);
    assert(agent_memory_forget(second_agent, &user, &change) == AGENT_ERROR_NOT_SUPPORTED && change == AGENT_MEMORY_UNCHANGED);
    assert(agent_memory_replace(agent, &user, SV(""), &change) == AGENT_OK);
    expect_document(agent, &user, "");
    assert(agent_file_size(&store, SV("USER.md"), &bytes) == AGENT_OK && bytes == 0u);
    assert(binding.ops.read(binding.context, &user, output, sizeof(output), 32u, &count) == AGENT_OK && count == 0u);
    assert(agent_file_append(&store, SV("USER.md"), "a\0b", 3u) == AGENT_OK);
    assert(agent_memory_read(agent, &user, output, sizeof(output), 32u, &view) == AGENT_ERROR_PARSE);
    assert(!view.data && output[0] == '\0');
    assert(agent_memory_forget(agent, &user, &change) == AGENT_OK && change == AGENT_MEMORY_APPLIED);
    assert(agent_memory_forget(agent, &facts, &change) == AGENT_OK);
    assert(agent_memory_forget(agent, &note, &change) == AGENT_OK);
    assert(agent_file_remove(&store, SV("SOUL.md")) == AGENT_OK);
    assert(agent_file_remove(&store, SV("session-61.jsonl")) == AGENT_OK);
    assert(agent_file_remove(&other_store, SV("USER.md")) == AGENT_OK);
    agent_destroy(agent); agent_destroy(second_agent);
    assert(rmdir(root) == 0 && rmdir(notes) == 0 && rmdir(other) == 0);
    return 0;
}
