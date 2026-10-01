/* SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200809L

#include <agent_posix_session_files.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SV(s) agent_string_view((s), sizeof(s) - 1u)

typedef struct {
    agent_message_view_t messages[8];
    agent_tool_call_view_t calls[4];
    char write_line[2048];
    char read_line[2048];
    char decoded[2048];
    union { uint64_t align; unsigned char bytes[2048]; } tokens;
    char session_id[64];
} jsonl_buffers_t;

static void write_side_file(const char* directory, const char* name)
{
    char path[256];
    FILE* file;
    int length = snprintf(path, sizeof(path), "%s/%s", directory, name);

    assert(length > 0 && (size_t)length < sizeof(path));
    file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite("keep", 1u, 4u, file) == 4u);
    assert(fclose(file) == 0);
}

static void remove_side_file(const char* directory, const char* name)
{
    char path[256];
    int length = snprintf(path, sizeof(path), "%s/%s", directory, name);

    assert(length > 0 && (size_t)length < sizeof(path));
    assert(unlink(path) == 0);
}

static void test_file_ops(const char* directory, agent_posix_session_files_t* files,
                          agent_session_jsonl_file_ops_t ops)
{
    static const char strange_id[] = {'.', '.', '/', 0, 'x'};
    char too_long[200];
    char output[5];
    uint64_t bytes = 123u;
    size_t count = 99u;

    memset(too_long, 'a', sizeof(too_long));
    memcpy(files->path, "alias", 5u);
    assert(ops.size(files, agent_string_view(files->path, 5u), &bytes) ==
           AGENT_ERROR_INVALID);
    assert(ops.size(files, SV("missing"), &bytes) == AGENT_ERROR_NOT_FOUND);
    assert(ops.remove(files, SV("missing")) == AGENT_ERROR_NOT_FOUND);
    assert(ops.sync(files, SV("missing")) == AGENT_ERROR_NOT_FOUND);
    assert(ops.append(files, agent_string_view(strange_id, sizeof(strange_id)),
                      "abc", 3u) == AGENT_OK);
    assert(strstr(files->path, "../") == NULL);
    assert(ops.size(files, agent_string_view(strange_id, sizeof(strange_id)),
                    &bytes) == AGENT_OK && bytes == 3u);
    assert(ops.read(files, agent_string_view(strange_id, sizeof(strange_id)),
                    0u, output, 3u) == AGENT_OK);
    assert(memcmp(output, "abc", 3u) == 0);
    assert(ops.read(files, agent_string_view(strange_id, sizeof(strange_id)),
                    2u, output, 2u) == AGENT_ERROR_IO);
    assert(ops.read(files, agent_string_view(strange_id, sizeof(strange_id)),
                    UINT64_MAX, output, 2u) == AGENT_ERROR_INVALID);
    assert(ops.truncate(files, agent_string_view(strange_id, sizeof(strange_id)),
                        UINT64_MAX) == AGENT_ERROR_INVALID);
    assert(ops.append(files, agent_string_view(too_long, sizeof(too_long)),
                      "x", 1u) == AGENT_ERROR_CAPACITY);
    assert(ops.sync(files, agent_string_view(strange_id, sizeof(strange_id))) == AGENT_OK);
    assert(ops.truncate(files, agent_string_view(strange_id, sizeof(strange_id)),
                        1u) == AGENT_OK);
    assert(ops.size(files, agent_string_view(strange_id, sizeof(strange_id)),
                    &bytes) == AGENT_OK && bytes == 1u);
    write_side_file(directory, "notes.md");
    write_side_file(directory, "session-zz.jsonl");
    assert(ops.append(files, SV("second"), "2", 1u) == AGENT_OK);
    assert(ops.append(files, SV("third"), "3", 1u) == AGENT_OK);
    assert(ops.count(files, &count) == AGENT_OK && count == 3u);
    assert(ops.clear_all(files) == AGENT_OK);
    assert(ops.count(files, &count) == AGENT_OK && count == 0u);
    remove_side_file(directory, "notes.md");
    remove_side_file(directory, "session-zz.jsonl");
}

static agent_error_t visit_group(void* context, const agent_session_group_view_t* group)
{
    size_t* visits = context;

    assert(group->message_count == 2u);
    assert(group->messages[0].role == AGENT_MESSAGE_ROLE_USER);
    assert(group->messages[1].role == AGENT_MESSAGE_ROLE_ASSISTANT);
    ++*visits;
    return AGENT_OK;
}

static void append_turn(agent_session_storage_t* storage, const char* input,
                        const char* answer)
{
    agent_message_view_t user = {0};
    agent_message_view_t assistant = {0};
    void* transaction = NULL;

    user.role = AGENT_MESSAGE_ROLE_USER;
    user.content = agent_string_view(input, strlen(input));
    assistant.role = AGENT_MESSAGE_ROLE_ASSISTANT;
    assistant.content = agent_string_view(answer, strlen(answer));
    assert(storage->ops.begin(storage->context, SV("chat"), &transaction) == AGENT_OK);
    assert(storage->ops.append(storage->context, transaction, &user) == AGENT_OK);
    assert(storage->ops.append(storage->context, transaction, &assistant) == AGENT_OK);
    assert(storage->ops.finish(storage->context, transaction,
                               AGENT_SESSION_TURN_COMPLETE) == AGENT_OK);
}

static void test_jsonl_integration(agent_posix_session_files_t* files,
                                    agent_session_jsonl_file_ops_t ops)
{
    jsonl_buffers_t buffers;
    agent_session_jsonl_config_t config = {0};
    agent_session_jsonl_t jsonl;
    agent_session_storage_t storage;
    size_t visits = 0u;
    size_t count = 0u;

    config.files = ops;
    config.file_context = files;
    config.write_line = buffers.write_line;
    config.write_line_capacity = sizeof(buffers.write_line);
    config.read_line = buffers.read_line;
    config.read_line_capacity = sizeof(buffers.read_line);
    config.decoded = buffers.decoded;
    config.decoded_capacity = sizeof(buffers.decoded);
    config.token_buffer = buffers.tokens.bytes;
    config.token_buffer_bytes = sizeof(buffers.tokens.bytes);
    config.read_messages = buffers.messages;
    config.read_message_capacity = 8u;
    config.read_calls = buffers.calls;
    config.read_call_capacity = 4u;
    config.session_id = buffers.session_id;
    config.session_id_capacity = sizeof(buffers.session_id);
    assert(agent_session_jsonl_init(&jsonl, &config) == AGENT_OK);
    assert(agent_session_jsonl_bind(&jsonl, &storage) == AGENT_OK);
    append_turn(&storage, "first", "one");
    assert(ops.append(files, SV("chat"), "{\"v\":", 5u) == AGENT_OK);
    assert(storage.ops.recent(storage.context, SV("chat"), 4u,
                              visit_group, &visits) == AGENT_OK && visits == 1u);
    append_turn(&storage, "second", "two");
    visits = 0u;
    assert(storage.ops.recent(storage.context, SV("chat"), 4u,
                              visit_group, &visits) == AGENT_OK && visits == 2u);
    assert(storage.ops.count(storage.context, &count) == AGENT_OK && count == 1u);
    assert(storage.ops.clear_all(storage.context) == AGENT_OK);
    assert(storage.ops.count(storage.context, &count) == AGENT_OK && count == 0u);
}

int main(void)
{
    char directory[] = "/tmp/cagent-posix-XXXXXX";
    char path[256];
    char short_path[8];
    agent_posix_session_files_t files = {0};
    agent_session_jsonl_file_ops_t ops;

    assert(mkdtemp(directory) != NULL);
    assert(agent_posix_session_files_init(&files, ".", path,
                                          sizeof(path)) == AGENT_ERROR_INVALID);
    assert(agent_posix_session_files_init(&files, directory, directory,
                                          sizeof(directory)) == AGENT_ERROR_INVALID);
    assert(agent_posix_session_files_init(&files, directory, short_path,
                                          sizeof(short_path)) == AGENT_ERROR_CAPACITY);
    assert(agent_posix_session_files_init(&files, directory, path,
                                          sizeof(path)) == AGENT_OK);
    ops = agent_posix_session_file_ops();
    test_file_ops(directory, &files, ops);
    test_jsonl_integration(&files, ops);
    assert(rmdir(directory) == 0);
    return 0;
}
