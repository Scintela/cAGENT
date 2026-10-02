/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 tanglinjie */
#define _POSIX_C_SOURCE 200809L
#include <agent_posix_file_store.h>
#include <agent_session_jsonl_files.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SV(s) agent_string_view((s), sizeof(s) - 1u)
typedef struct {
    agent_message_view_t messages[8];
    agent_tool_call_view_t calls[4];
    char write_line[2048], read_line[2048], decoded[2048], session_id[64];
    union { uint64_t align; unsigned char bytes[2048]; } tokens;
} jsonl_buffers_t;

static agent_error_t count_names(void* ctx, agent_string_view_t name, bool* stop)
{
    size_t* count = ctx;
    (void)stop;
    assert(name.size && name.data[0] != '.');
    ++*count;
    return AGENT_OK;
}

static agent_error_t stop_names(void* ctx, agent_string_view_t name, bool* stop)
{
    size_t* count = ctx;
    (void)name;
    ++*count;
    *stop = true;
    return AGENT_OK;
}

static agent_error_t fail_names(void* ctx, agent_string_view_t name, bool* stop)
{
    (void)ctx; (void)name; (void)stop;
    return AGENT_ERROR_CANCELLED;
}

static agent_error_t group(void* ctx, const agent_session_group_view_t* value)
{
    size_t* count = ctx;
    assert(value->message_count == 2u);
    assert(value->messages[0].role == AGENT_MESSAGE_ROLE_USER);
    assert(value->messages[1].role == AGENT_MESSAGE_ROLE_ASSISTANT);
    ++*count;
    return AGENT_OK;
}

static void append_turn(agent_session_storage_t* storage)
{
    agent_message_view_t user = {0}, assistant = {0};
    void* transaction = NULL;
    user.role = AGENT_MESSAGE_ROLE_USER; user.content = SV("hello");
    assistant.role = AGENT_MESSAGE_ROLE_ASSISTANT; assistant.content = SV("world");
    assert(storage->ops.begin(storage->context, SV("chat"), &transaction) == AGENT_OK);
    assert(storage->ops.append(storage->context, transaction, &user) == AGENT_OK);
    assert(storage->ops.append(storage->context, transaction, &assistant) == AGENT_OK);
    assert(storage->ops.finish(storage->context, transaction, AGENT_SESSION_TURN_COMPLETE) == AGENT_OK);
}

static void jsonl_chain(agent_file_store_t* store)
{
    agent_session_jsonl_files_t files;
    agent_session_jsonl_config_t config = {0};
    agent_session_jsonl_t jsonl;
    agent_session_storage_t storage;
    jsonl_buffers_t buffers;
    char name[160], output[16];
    const char unusual_id[] = {'/', '\0', '.'};
    agent_string_view_t id = agent_string_view(unusual_id, sizeof(unusual_id));
    size_t count = 0u;
    config.write_line = buffers.write_line; config.write_line_capacity = sizeof(buffers.write_line);
    config.read_line = buffers.read_line; config.read_line_capacity = sizeof(buffers.read_line);
    config.decoded = buffers.decoded; config.decoded_capacity = sizeof(buffers.decoded);
    config.token_buffer = buffers.tokens.bytes; config.token_buffer_bytes = sizeof(buffers.tokens.bytes);
    config.read_messages = buffers.messages; config.read_message_capacity = 8u;
    config.read_calls = buffers.calls; config.read_call_capacity = 4u;
    config.session_id = buffers.session_id; config.session_id_capacity = sizeof(buffers.session_id);
    assert(agent_session_jsonl_files_init(&files, store, name, sizeof(name), &config) == AGENT_OK);
    assert(config.files.append(&files, id, "data", 4u) == AGENT_OK);
    assert(config.files.read(&files, id, 0u, output, 4u) == AGENT_OK && !memcmp(output, "data", 4u));
    assert(config.files.read(&files, id, 0u, name, 4u) == AGENT_ERROR_INVALID);
    assert(config.files.append(&files, id, name, 4u) == AGENT_ERROR_INVALID);
    assert(config.files.read(&files, id, 0u, output, 5u) == AGENT_ERROR_IO);
    assert(config.files.remove(&files, id) == AGENT_OK);
    assert(agent_session_jsonl_init(&jsonl, &config) == AGENT_OK);
    assert(agent_session_jsonl_bind(&jsonl, &storage) == AGENT_OK);
    append_turn(&storage);
    assert(config.files.append(&files, SV("chat"), "{\"v\":", 5u) == AGENT_OK);
    assert(storage.ops.recent(storage.context, SV("chat"), 4u, group, &count) == AGENT_OK && count == 1u);
    append_turn(&storage);
    count = 0u;
    assert(storage.ops.recent(storage.context, SV("chat"), 4u, group, &count) == AGENT_OK && count == 2u);
    assert(storage.ops.count(storage.context, &count) == AGENT_OK && count == 1u);
    assert(agent_file_append(store, SV("session-zz.jsonl"), "keep", 4u) == AGENT_OK);
    assert(storage.ops.clear_all(storage.context) == AGENT_OK);
    assert(storage.ops.count(storage.context, &count) == AGENT_OK && count == 0u);
    assert(agent_file_read_exact(store, SV("USER.md"), 0u, output, 3u) == AGENT_OK);
    assert(agent_file_read_exact(store, SV("session-zz.jsonl"), 0u, output, 4u) == AGENT_OK);
    assert(agent_file_remove(store, SV("session-zz.jsonl")) == AGENT_OK);
}

int main(void)
{
    char root[] = "/tmp/cagent-files-XXXXXX", other[] = "/tmp/cagent-files-other-XXXXXX";
    char scratch[512], readonly_scratch[512], tiny[128], output[32], link_path[128];
    agent_posix_file_store_t state, readonly;
    agent_file_store_t store, reader;
    agent_posix_file_store_config_t config = {root, scratch, sizeof(scratch), false, true};
    agent_string_view_t text;
    bool published;
    uint64_t length;
    size_t count;
    assert(mkdtemp(root) && mkdtemp(other));
    assert(agent_posix_file_store_init(&state, &config, &store) == AGENT_OK);
    assert(agent_file_size(&store, SV("USER.md"), &length) == AGENT_ERROR_NOT_FOUND);
    assert(agent_file_append(&store, SV("USER.md"), "old", 3u) == AGENT_OK);
    assert(agent_file_sync(&store, SV("USER.md")) == AGENT_OK);
    assert(agent_file_read(&store, SV("USER.md"), 2u, output, sizeof(output), &count) == AGENT_OK && count == 1u);
    assert(agent_file_read(&store, SV("USER.md"), UINT64_MAX, output, 0u, &count) == AGENT_ERROR_INVALID);
    assert(agent_file_read(&store, SV("USER.md"), 0u, scratch, 3u, &count) == AGENT_ERROR_INVALID);
    assert(agent_file_append(&store, SV("USER.md"), scratch, 3u) == AGENT_ERROR_INVALID);
    assert(agent_file_replace(&store, SV("USER.md"), "new value", 9u, &published) == AGENT_OK && published);
    assert(agent_file_read_text(&store, SV("USER.md"), output, sizeof(output), 16u, &text) == AGENT_OK);
    assert(text.size == 9u && !strcmp(output, "new value"));
    assert(agent_file_read_text(&store, SV("USER.md"), output, sizeof(output), 8u, &text) == AGENT_ERROR_CAPACITY);
    assert(text.data == NULL && output[0] == '\0');
    assert(agent_file_truncate(&store, SV("USER.md"), 3u) == AGENT_OK);
    assert(agent_file_truncate(&store, SV("USER.md"), UINT64_MAX) == AGENT_ERROR_INVALID);
    assert(agent_file_append(&store, SV("USER.md"), NULL, 0u) == AGENT_OK);
    count = 0u;
    assert(agent_file_visit(&store, count_names, &count) == AGENT_OK && count == 1u);
    count = 0u;
    assert(agent_file_visit(&store, stop_names, &count) == AGENT_OK && count == 1u);
    assert(agent_file_visit(&store, fail_names, NULL) == AGENT_ERROR_CANCELLED);
    assert(snprintf(link_path, sizeof(link_path), "%s/link.md", root) > 0);
    assert(symlink("USER.md", link_path) == 0);
    assert(agent_file_size(&store, SV("link.md"), &length) == AGENT_ERROR_IO);
    assert(agent_file_replace(&store, SV("link.md"), "x", 1u, &published) == AGENT_ERROR_IO && !published);
    count = 0u;
    assert(agent_file_visit(&store, count_names, &count) == AGENT_OK && count == 1u);
    assert(unlink(link_path) == 0);
    config.read_only = true; config.sync_directory = false; config.path_buffer = readonly_scratch;
    assert(agent_posix_file_store_init(&readonly, &config, &reader) == AGENT_OK);
    assert(agent_file_read_text(&reader, SV("USER.md"), output, sizeof(output), 8u, &text) == AGENT_OK);
    assert(agent_file_append(&reader, SV("USER.md"), "x", 1u) == AGENT_ERROR_NOT_SUPPORTED);
    assert(agent_file_replace(&reader, SV("USER.md"), "x", 1u, &published) == AGENT_ERROR_NOT_SUPPORTED);
    {
        agent_session_jsonl_files_t files;
        agent_session_jsonl_config_t jsonl_config = {0};
        char name[160];
        assert(agent_session_jsonl_files_init(&files, &reader, name, sizeof(name), &jsonl_config) == AGENT_ERROR_NOT_SUPPORTED);
    }
    config.directory = other;
    assert(agent_posix_file_store_init(&readonly, &config, &reader) == AGENT_OK);
    assert(agent_file_size(&reader, SV("USER.md"), &length) == AGENT_ERROR_NOT_FOUND);
    config.directory = root; config.path_buffer = tiny; config.read_only = false;
    config.path_capacity = strlen(root) + 1u + sizeof("USER.md");
    assert(agent_posix_file_store_init(&readonly, &config, &reader) == AGENT_OK);
    assert(agent_file_replace(&reader, SV("USER.md"), "x", 1u, &published) == AGENT_ERROR_CAPACITY && !published);
    assert(agent_file_read_exact(&store, SV("USER.md"), 0u, output, 3u) == AGENT_OK && !memcmp(output, "new", 3u));
    jsonl_chain(&store);
    assert(agent_file_remove(&store, SV("USER.md")) == AGENT_OK);
    assert(agent_file_remove(&store, SV("USER.md")) == AGENT_ERROR_NOT_FOUND);
    assert(rmdir(root) == 0 && rmdir(other) == 0);
    return 0;
}
