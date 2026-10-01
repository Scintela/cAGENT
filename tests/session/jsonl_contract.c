/* SPDX-License-Identifier: MIT */
#define _XOPEN_SOURCE 700

#include <agent_session_jsonl.h>

#include "core/arena_internal.h"
#include "session/session_internal.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define SV(s) agent_string_view((s), sizeof(s) - 1u)

typedef struct {
    char directory[64];
    bool fail_append;
} file_context_t;

static agent_error_t file_path(const file_context_t* files, agent_string_view_t id,
                               char path[512])
{
    static const char digits[] = "0123456789abcdef";
    size_t base = strlen(files->directory);
    size_t i;

    if (!id.data || id.size == 0u || base + id.size * 2u + 8u > 512u)
        return AGENT_ERROR_INVALID;
    memcpy(path, files->directory, base);
    path[base++] = '/';
    for (i = 0u; i < id.size; ++i) {
        unsigned char byte = (unsigned char)id.data[i];
        path[base++] = digits[byte >> 4u];
        path[base++] = digits[byte & 15u];
    }
    memcpy(path + base, ".jsonl", 7u);
    return AGENT_OK;
}

static agent_error_t fs_size(void* context, agent_string_view_t id, uint64_t* bytes)
{
    char path[512];
    FILE* stream;
    long length;

    if (file_path(context, id, path) != AGENT_OK || !bytes) return AGENT_ERROR_INVALID;
    stream = fopen(path, "rb");
    if (!stream) return errno == ENOENT ? AGENT_ERROR_NOT_FOUND : AGENT_ERROR_IO;
    if (fseek(stream, 0, SEEK_END) != 0 || (length = ftell(stream)) < 0) {
        fclose(stream);
        return AGENT_ERROR_IO;
    }
    fclose(stream);
    *bytes = (uint64_t)length;
    return AGENT_OK;
}

static agent_error_t fs_read(void* context, agent_string_view_t id, uint64_t offset,
                             char* output, size_t size)
{
    char path[512];
    FILE* stream;
    size_t got;

    if (file_path(context, id, path) != AGENT_OK || offset > LONG_MAX)
        return AGENT_ERROR_INVALID;
    stream = fopen(path, "rb");
    if (!stream) return AGENT_ERROR_IO;
    if (fseek(stream, (long)offset, SEEK_SET) != 0) {
        fclose(stream);
        return AGENT_ERROR_IO;
    }
    got = fread(output, 1u, size, stream);
    fclose(stream);
    return got == size ? AGENT_OK : AGENT_ERROR_IO;
}

static agent_error_t fs_append(void* context, agent_string_view_t id,
                               const char* data, size_t size)
{
    file_context_t* files = context;
    char path[512];
    FILE* stream;
    size_t amount = size;
    size_t written;
    int close_status;
    bool fail = files->fail_append;

    if (file_path(files, id, path) != AGENT_OK) return AGENT_ERROR_INVALID;
    files->fail_append = false;
    stream = fopen(path, "ab");
    if (!stream) return AGENT_ERROR_IO;
    if (fail) amount = size / 2u;
    written = fwrite(data, 1u, amount, stream);
    close_status = fclose(stream);
    if (written != amount || close_status != 0)
        return AGENT_ERROR_IO;
    return fail ? AGENT_ERROR_IO : AGENT_OK;
}

static agent_error_t fs_truncate(void* context, agent_string_view_t id, uint64_t bytes)
{
    char path[512];

    if (file_path(context, id, path) != AGENT_OK || bytes > LONG_MAX)
        return AGENT_ERROR_INVALID;
    return truncate(path, (off_t)bytes) == 0 ? AGENT_OK : AGENT_ERROR_IO;
}

static agent_error_t fs_sync(void* context, agent_string_view_t id)
{
    char path[512];
    FILE* stream;
    int status;

    if (file_path(context, id, path) != AGENT_OK) return AGENT_ERROR_INVALID;
    stream = fopen(path, "rb");
    if (!stream) return AGENT_ERROR_IO;
    status = fsync(fileno(stream));
    fclose(stream);
    return status == 0 ? AGENT_OK : AGENT_ERROR_IO;
}

static agent_error_t fs_remove(void* context, agent_string_view_t id)
{
    char path[512];

    if (file_path(context, id, path) != AGENT_OK) return AGENT_ERROR_INVALID;
    return unlink(path) == 0 ? AGENT_OK : AGENT_ERROR_NOT_FOUND;
}

static agent_error_t fs_walk(void* context, bool remove_files, size_t* count)
{
    file_context_t* files = context;
    DIR* directory = opendir(files->directory);
    struct dirent* item;

    if (!directory) return AGENT_ERROR_IO;
    *count = 0u;
    while ((item = readdir(directory)) != NULL) {
        size_t length = strlen(item->d_name);
        char path[512];

        if (length < 6u || strcmp(item->d_name + length - 6u, ".jsonl") != 0)
            continue;
        ++*count;
        if (remove_files) {
            int result = snprintf(path, sizeof(path), "%s/%s",
                                  files->directory, item->d_name);
            if (result < 0 || (size_t)result >= sizeof(path) || unlink(path) != 0) {
                closedir(directory);
                return AGENT_ERROR_IO;
            }
        }
    }
    closedir(directory);
    return AGENT_OK;
}

static agent_error_t fs_clear_all(void* context)
{
    size_t count;
    return fs_walk(context, true, &count);
}

static agent_error_t fs_count(void* context, size_t* count)
{
    return fs_walk(context, false, count);
}

static agent_session_jsonl_file_ops_t file_ops(void)
{
    agent_session_jsonl_file_ops_t ops = {
        fs_size, fs_read, fs_append, fs_truncate, fs_sync,
        fs_remove, fs_clear_all, fs_count
    };
    return ops;
}

typedef struct {
    agent_message_view_t messages[16];
    agent_tool_call_view_t calls[8];
    char write_line[4096];
    char read_line[4096];
    char decoded[4096];
    union { uint64_t align; unsigned char bytes[4096]; } tokens;
    char id[64];
} workspace_t;

static void configure(agent_session_jsonl_config_t* config,
                      file_context_t* files, workspace_t* workspace)
{
    memset(config, 0, sizeof(*config));
    config->files = file_ops();
    config->file_context = files;
    config->write_line = workspace->write_line;
    config->write_line_capacity = sizeof(workspace->write_line);
    config->read_line = workspace->read_line;
    config->read_line_capacity = sizeof(workspace->read_line);
    config->decoded = workspace->decoded;
    config->decoded_capacity = sizeof(workspace->decoded);
    config->token_buffer = workspace->tokens.bytes;
    config->token_buffer_bytes = sizeof(workspace->tokens.bytes);
    config->read_messages = workspace->messages;
    config->read_message_capacity = 16u;
    config->read_calls = workspace->calls;
    config->read_call_capacity = 8u;
    config->session_id = workspace->id;
    config->session_id_capacity = sizeof(workspace->id);
}

typedef struct {
    size_t seen;
    size_t message_counts[4];
    char first_content[4];
} visitor_t;

static agent_error_t visit(void* context, const agent_session_group_view_t* group)
{
    visitor_t* visitor = context;

    assert(visitor->seen < 4u && group->message_count > 0u);
    visitor->message_counts[visitor->seen] = group->message_count;
    visitor->first_content[visitor->seen] = group->messages[0].content.data[0];
    ++visitor->seen;
    return AGENT_OK;
}

static agent_message_view_t msg(agent_message_role_t role, agent_string_view_t text)
{
    agent_message_view_t message = {0};
    message.role = role;
    message.content = text;
    return message;
}

static void test_filesystem_round_trip(void)
{
    char directory[] = "/tmp/cagent-jsonl-XXXXXX";
    file_context_t files = {{0}, false};
    static workspace_t workspace;
    static unsigned char scratch[AGENT_SCRATCH_BYTES];
    agent_session_jsonl_config_t config;
    agent_session_jsonl_t jsonl;
    agent_session_storage_t storage;
    agent_session_turn_t* turn;
    agent_arena_t arena;
    agent_tool_call_view_t call = {SV("id-1"), SV("lamp"), SV("{\"on\":true}")};
    agent_message_view_t assistant = msg(AGENT_MESSAGE_ROLE_ASSISTANT, SV(""));
    agent_message_view_t result = msg(AGENT_MESSAGE_ROLE_TOOL, SV("on"));
    agent_message_view_t final = msg(AGENT_MESSAGE_ROLE_ASSISTANT, SV("done\nnow"));
    const agent_message_view_t* projected;
    size_t projected_count;
    size_t count;
    visitor_t seen = {0};

    assert(mkdtemp(directory));
    strcpy(files.directory, directory);
    configure(&config, &files, &workspace);
    assert(agent_session_jsonl_init(&jsonl, &config) == AGENT_OK);
    assert(agent_session_jsonl_bind(&jsonl, &storage) == AGENT_OK);
    assert(agent_arena_init(&arena, scratch, sizeof(scratch)) == AGENT_OK);
    assert(storage.ops.count(storage.context, &count) == AGENT_OK && count == 0u);

    assistant.tool_calls = &call;
    assistant.tool_call_count = 1u;
    result.tool_call_id = SV("id-1");
    assert(agent_session_turn_open(&turn, &arena, &storage, SV("home"), SV("hello")) == AGENT_OK);
    assert(agent_session_append(turn, &assistant) == AGENT_OK);
    assert(agent_session_append(turn, &result) == AGENT_OK);
    assert(agent_session_append(turn, &final) == AGENT_OK);
    assert(agent_session_turn_finish(turn, AGENT_SESSION_TURN_COMPLETE) == AGENT_OK);
    assert(agent_arena_rewind(&arena, 0u) == AGENT_OK);

    assert(agent_session_jsonl_init(&jsonl, &config) == AGENT_OK);
    assert(agent_session_jsonl_bind(&jsonl, &storage) == AGENT_OK);
    assert(storage.ops.count(storage.context, &count) == AGENT_OK && count == 1u);
    assert(storage.ops.recent(storage.context, SV("home"), 2u, visit, &seen) == AGENT_OK);
    assert(seen.seen == 1u && seen.message_counts[0] == 4u &&
           seen.first_content[0] == 'h');
    assert(agent_session_turn_open(&turn, &arena, &storage, SV("home"), SV("second")) == AGENT_OK);
    assert(agent_session_project(turn, &arena, 1u, &projected, &projected_count) == AGENT_OK);
    assert(projected_count == 5u && projected[1].tool_call_count == 1u &&
           projected[2].role == AGENT_MESSAGE_ROLE_TOOL);
    assert(projected[1].tool_calls[0].arguments_json.size == call.arguments_json.size &&
           memcmp(projected[1].tool_calls[0].arguments_json.data,
                  call.arguments_json.data, call.arguments_json.size) == 0);
    assert(projected[3].content.size == final.content.size &&
           memcmp(projected[3].content.data, final.content.data, final.content.size) == 0);
    assert(agent_session_turn_finish(turn, AGENT_SESSION_TURN_ABORTED) == AGENT_OK);
    assert(agent_arena_rewind(&arena, 0u) == AGENT_OK);

    seen.seen = 0u;
    assert(storage.ops.recent(storage.context, SV("home"), 2u, visit, &seen) == AGENT_OK);
    assert(seen.seen == 1u);
    assert(agent_session_turn_open(&turn, &arena, &storage, SV("other"), SV("other user")) == AGENT_OK);
    assert(agent_session_append(turn, &final) == AGENT_OK);
    assert(agent_session_turn_finish(turn, AGENT_SESSION_TURN_COMPLETE) == AGENT_OK);
    assert(agent_arena_rewind(&arena, 0u) == AGENT_OK);
    assert(storage.ops.count(storage.context, &count) == AGENT_OK && count == 2u);
    seen.seen = 0u;
    assert(storage.ops.recent(storage.context, SV("home"), 2u, visit, &seen) == AGENT_OK);
    assert(seen.seen == 1u);
    assert(storage.ops.clear(storage.context, SV("home")) == AGENT_OK);
    assert(storage.ops.count(storage.context, &count) == AGENT_OK && count == 2u);
    assert(storage.ops.remove(storage.context, SV("home")) == AGENT_OK);
    assert(storage.ops.count(storage.context, &count) == AGENT_OK && count == 1u);
    assert(storage.ops.clear_all(storage.context) == AGENT_OK);
    assert(storage.ops.count(storage.context, &count) == AGENT_OK && count == 0u);
    assert(rmdir(directory) == 0);
}

static void test_tail_recovery_and_failure(void)
{
    char directory[] = "/tmp/cagent-jsonl-XXXXXX";
    file_context_t files = {{0}, false};
    static workspace_t workspace;
    agent_session_jsonl_config_t config;
    agent_session_jsonl_t jsonl;
    agent_session_storage_t storage;
    agent_message_view_t user = msg(AGENT_MESSAGE_ROLE_USER, SV("first"));
    agent_message_view_t final = msg(AGENT_MESSAGE_ROLE_ASSISTANT, SV("ok"));
    visitor_t seen = {0};
    void* transaction;
    char path[512];

    assert(mkdtemp(directory));
    strcpy(files.directory, directory);
    configure(&config, &files, &workspace);
    assert(agent_session_jsonl_init(&jsonl, &config) == AGENT_OK);
    assert(agent_session_jsonl_bind(&jsonl, &storage) == AGENT_OK);
    assert(storage.ops.begin(storage.context, SV("x"), &transaction) == AGENT_OK);
    assert(storage.ops.append(storage.context, transaction, &user) == AGENT_OK);
    assert(storage.ops.append(storage.context, transaction, &final) == AGENT_OK);
    assert(storage.ops.finish(storage.context, transaction, AGENT_SESSION_TURN_COMPLETE) == AGENT_OK);
    assert(fs_append(&files, SV("x"), "{\"v\":1", 6u) == AGENT_OK);
    assert(storage.ops.recent(storage.context, SV("x"), 2u, visit, &seen) == AGENT_OK);
    assert(seen.seen == 1u);
    assert(storage.ops.begin(storage.context, SV("x"), &transaction) == AGENT_OK);
    assert(storage.ops.append(storage.context, transaction, &user) == AGENT_OK);
    assert(storage.ops.append(storage.context, transaction, &final) == AGENT_OK);
    files.fail_append = true;
    assert(storage.ops.finish(storage.context, transaction, AGENT_SESSION_TURN_COMPLETE) == AGENT_ERROR_IO);
    seen.seen = 0u;
    assert(storage.ops.recent(storage.context, SV("x"), 3u, visit, &seen) == AGENT_OK);
    assert(seen.seen == 1u);
    assert(storage.ops.begin(storage.context, SV("x"), &transaction) == AGENT_OK);
    assert(storage.ops.append(storage.context, transaction, &user) == AGENT_OK);
    assert(storage.ops.append(storage.context, transaction, &final) == AGENT_OK);
    assert(storage.ops.finish(storage.context, transaction, AGENT_SESSION_TURN_COMPLETE) == AGENT_OK);
    seen.seen = 0u;
    assert(storage.ops.recent(storage.context, SV("x"), 3u, visit, &seen) == AGENT_OK);
    assert(seen.seen == 2u);

    assert(fs_append(&files, SV("x"), "{}\n", 3u) == AGENT_OK);
    assert(storage.ops.recent(storage.context, SV("x"), 3u, visit, &seen) == AGENT_ERROR_PARSE);
    assert(file_path(&files, SV("x"), path) == AGENT_OK);
    assert(truncate(path, 0) == 0);
    assert(storage.ops.clear_all(storage.context) == AGENT_OK);
    assert(rmdir(directory) == 0);
}

static void test_capacity(void)
{
    char directory[] = "/tmp/cagent-jsonl-XXXXXX";
    file_context_t files = {{0}, false};
    static workspace_t workspace;
    agent_session_jsonl_config_t config;
    agent_session_jsonl_t jsonl;
    agent_session_storage_t storage;
    agent_message_view_t user = msg(AGENT_MESSAGE_ROLE_USER, SV("too long for buffer"));
    agent_message_view_t short_user = msg(AGENT_MESSAGE_ROLE_USER, SV("a"));
    void* transaction;

    assert(mkdtemp(directory));
    strcpy(files.directory, directory);
    configure(&config, &files, &workspace);
    config.write_line_capacity = 64u;
    assert(agent_session_jsonl_init(&jsonl, &config) == AGENT_OK);
    assert(agent_session_jsonl_bind(&jsonl, &storage) == AGENT_OK);
    assert(storage.ops.begin(storage.context, SV("x"), &transaction) == AGENT_OK);
    assert(storage.ops.append(storage.context, transaction, &user) == AGENT_ERROR_CAPACITY);
    assert(storage.ops.append(storage.context, transaction, &short_user) == AGENT_OK);
    assert(storage.ops.finish(storage.context, transaction, AGENT_SESSION_TURN_ABORTED) == AGENT_OK);
    assert(storage.ops.clear_all(storage.context) == AGENT_OK);
    assert(rmdir(directory) == 0);
}

int main(void)
{
    test_filesystem_round_trip();
    test_tail_recovery_and_failure();
    test_capacity();
    return 0;
}
