/* SPDX-License-Identifier: MIT */
#include <agent.h>
#include <agent_session_ram.h>

#include "core/arena_internal.h"
#include "session/session_internal.h"

#include <assert.h>
#include <string.h>

#define SV(literal) agent_string_view((literal), sizeof(literal) - 1u)

static uint64_t now_ms(void* context)
{
    (void)context;
    return 1u;
}

static agent_message_view_t message(agent_message_role_t role,
                                    agent_string_view_t content)
{
    agent_message_view_t result = {0};
    result.role = role;
    result.content = content;
    return result;
}

static agent_error_t (*real_append)(void*, void*, const agent_message_view_t*);
static bool fail_next_append;

static agent_error_t injected_append(void* context, void* transaction,
                                     const agent_message_view_t* item)
{
    if (fail_next_append) {
        fail_next_append = false;
        return AGENT_ERROR_IO;
    }
    return real_append(context, transaction, item);
}

static void test_transactions_and_projection(void)
{
    static agent_workspace_t workspace;
    static unsigned char scratch[AGENT_SCRATCH_BYTES];
    static agent_session_ram_turn_t turns[8];
    static agent_session_ram_message_t stored_messages[32];
    static agent_session_ram_call_t stored_calls[12];
    static agent_message_view_t read_messages[16];
    static agent_tool_call_view_t read_calls[8];
    static char payload[4096];
    agent_session_ram_config_t ram_config = {
        turns, 8u, stored_messages, 32u, stored_calls, 12u,
        payload, sizeof(payload), read_messages, 16u, read_calls, 8u
    };
    agent_config_t config = agent_config_default();
    agent_session_ram_t ram;
    agent_session_storage_t storage;
    agent_arena_t arena;
    agent_t* agent;
    agent_session_turn_t* turn;
    agent_tool_call_view_t call = {SV("call-1"), SV("lamp"), SV("{\"on\":true}")};
    agent_message_view_t tool_call = message(AGENT_MESSAGE_ROLE_ASSISTANT, SV(""));
    agent_message_view_t tool_result = message(AGENT_MESSAGE_ROLE_TOOL, SV("on"));
    agent_message_view_t final = message(AGENT_MESSAGE_ROLE_ASSISTANT, SV("done"));
    agent_message_view_t other_final = message(AGENT_MESSAGE_ROLE_ASSISTANT, SV("other done"));
    const agent_message_view_t* projected;
    size_t projected_count;
    size_t projection_mark;
    size_t session_count;
    char user[] = "hello";

    config.runtime.now_ms = now_ms;
    assert(agent_session_ram_init(&ram, &ram_config) == AGENT_OK);
    assert(agent_session_ram_bind(&ram, &storage) == AGENT_OK);
    assert(agent_init(&agent, &workspace, &config) == AGENT_OK);
    assert(agent_set_session_storage(agent, &storage) == AGENT_OK);
    assert(agent_start(agent) == AGENT_OK);
    assert(agent_session_count(agent, &session_count) == AGENT_OK && session_count == 0u);
    assert(agent_arena_init(&arena, scratch, sizeof(scratch)) == AGENT_OK);

    assert(agent_session_turn_open(&turn, &arena, &storage, SV("home"),
                                   agent_string_view(user, strlen(user))) == AGENT_OK);
    user[0] = 'X';
    projection_mark = arena.used;
    assert(agent_session_project(turn, &arena, 0u, &projected, &projected_count) == AGENT_OK);
    assert(projected_count == 1u && projected[0].content.data[0] == 'h');
    assert(agent_arena_rewind(&arena, projection_mark) == AGENT_OK);
    tool_call.tool_calls = &call;
    tool_call.tool_call_count = 1u;
    assert(agent_session_append(turn, &tool_call) == AGENT_OK);
    assert(agent_session_append(turn, &final) == AGENT_ERROR_INVALID);
    tool_result.tool_call_id = SV("call-1");
    assert(agent_session_append(turn, &tool_result) == AGENT_OK);
    assert(agent_session_append(turn, &tool_result) == AGENT_ERROR_EXISTS);
    assert(agent_session_append(turn, &tool_call) == AGENT_ERROR_EXISTS);
    assert(agent_session_append(turn, &final) == AGENT_OK);
    assert(agent_session_turn_finish(turn, AGENT_SESSION_TURN_COMPLETE) == AGENT_OK);
    assert(agent_arena_rewind(&arena, 0u) == AGENT_OK);

    assert(agent_session_turn_open(&turn, &arena, &storage, SV("home"), SV("second")) == AGENT_OK);
    assert(agent_session_append(turn, &final) == AGENT_OK);
    assert(agent_session_turn_finish(turn, AGENT_SESSION_TURN_COMPLETE) == AGENT_OK);
    assert(agent_arena_rewind(&arena, 0u) == AGENT_OK);

    assert(agent_session_turn_open(&turn, &arena, &storage, SV("home"), SV("next")) == AGENT_OK);
    projection_mark = arena.used;
    payload[stored_messages[2].tool_id_offset] = 'x';
    assert(agent_session_project(turn, &arena, 2u, &projected, &projected_count) ==
           AGENT_ERROR_PARSE);
    assert(arena.used == projection_mark);
    payload[stored_messages[2].tool_id_offset] = 'c';
    assert(agent_session_project(turn, &arena, 2u, &projected, &projected_count) == AGENT_OK);
    assert(projected_count == 7u);
    assert(projected[0].role == AGENT_MESSAGE_ROLE_USER &&
           projected[0].content.data[0] == 'h');
    assert(projected[1].tool_call_count == 1u &&
           projected[1].tool_calls[0].arguments_json.size == call.arguments_json.size);
    assert(projected[2].role == AGENT_MESSAGE_ROLE_TOOL);
    assert(projected[3].role == AGENT_MESSAGE_ROLE_ASSISTANT);
    assert(projected[4].role == AGENT_MESSAGE_ROLE_USER);
    assert(projected[4].content.size == SV("second").size);
    assert(projected[6].role == AGENT_MESSAGE_ROLE_USER);
    assert(agent_arena_rewind(&arena, projection_mark) == AGENT_OK);
    assert(agent_session_project(turn, &arena, 1u, &projected, &projected_count) == AGENT_OK);
    assert(projected_count == 3u && projected[0].content.size == SV("second").size);
    assert(agent_arena_rewind(&arena, projection_mark) == AGENT_OK);
    assert(agent_session_project(turn, &arena, 0u, &projected, &projected_count) == AGENT_OK);
    assert(projected_count == 1u);
    assert(agent_arena_rewind(&arena, projection_mark) == AGENT_OK);
    assert(agent_session_turn_finish(turn, AGENT_SESSION_TURN_ABORTED) == AGENT_OK);
    assert(agent_arena_rewind(&arena, 0u) == AGENT_OK);

    real_append = storage.ops.append;
    storage.ops.append = injected_append;
    assert(agent_session_turn_open(&turn, &arena, &storage, SV("other"), SV("question")) == AGENT_OK);
    assert(agent_session_append(turn, &tool_call) == AGENT_OK);
    assert(agent_session_append(turn, &tool_result) == AGENT_OK);
    fail_next_append = true;
    assert(agent_session_append(turn, &other_final) == AGENT_ERROR_IO);
    assert(agent_session_append(turn, &other_final) == AGENT_OK);
    assert(agent_session_turn_finish(turn, AGENT_SESSION_TURN_COMPLETE) == AGENT_OK);
    storage.ops.append = real_append;
    assert(agent_arena_rewind(&arena, 0u) == AGENT_OK);
    assert(agent_session_count(agent, &session_count) == AGENT_OK && session_count == 2u);
    assert(agent_session_clear(agent, SV("home")) == AGENT_OK);
    assert(agent_session_count(agent, &session_count) == AGENT_OK && session_count == 1u);
    assert(agent_session_turn_open(&turn, &arena, &storage, SV("other"), SV("again")) == AGENT_OK);
    assert(agent_session_project(turn, &arena, 2u, &projected, &projected_count) == AGENT_OK);
    assert(projected_count == 5u && projected[0].content.size == SV("question").size);
    assert(projected[1].tool_call_count == 1u &&
           memcmp(projected[1].tool_calls[0].id.data, "call-1", 6u) == 0);
    assert(agent_session_turn_finish(turn, AGENT_SESSION_TURN_ABORTED) == AGENT_OK);
    assert(agent_arena_rewind(&arena, 0u) == AGENT_OK);
    assert(agent_session_remove(agent, SV("other")) == AGENT_OK);
    assert(agent_session_count(agent, &session_count) == AGENT_OK && session_count == 0u);
    assert(agent_session_clear_all(agent) == AGENT_OK);
    agent_destroy(agent);
}

static void test_capacity_and_missing_storage(void)
{
    static unsigned char scratch[AGENT_SCRATCH_BYTES];
    static agent_session_ram_turn_t turns[1];
    static agent_session_ram_message_t stored_messages[2];
    static agent_session_ram_call_t stored_calls[1];
    static agent_message_view_t read_messages[2];
    static agent_tool_call_view_t read_calls[1];
    static char payload[8];
    agent_session_ram_config_t config = {
        turns, 1u, stored_messages, 2u, stored_calls, 1u,
        payload, sizeof(payload), read_messages, 2u, read_calls, 1u
    };
    agent_session_ram_t ram;
    agent_session_storage_t storage;
    agent_arena_t arena;
    agent_session_turn_t* turn;
    const agent_message_view_t* projected;
    size_t count;

    assert(agent_session_ram_init(&ram, &config) == AGENT_OK);
    assert(agent_session_ram_bind(&ram, &storage) == AGENT_OK);
    assert(agent_arena_init(&arena, scratch, sizeof(scratch)) == AGENT_OK);
    assert(agent_session_turn_open(&turn, &arena, NULL, SV(""), SV("x")) == AGENT_OK);
    assert(agent_session_project(turn, &arena, 1u, &projected, &count) ==
           AGENT_ERROR_NOT_SUPPORTED);
    assert(agent_session_turn_finish(turn, AGENT_SESSION_TURN_ABORTED) == AGENT_OK);
    assert(agent_arena_rewind(&arena, 0u) == AGENT_OK);
    assert(agent_session_turn_open(&turn, &arena, &storage, SV("home"), SV("long input")) ==
           AGENT_ERROR_CAPACITY);
    assert(arena.used == 0u);
    assert(ram.turn_count == 1u && !ram.active &&
           ram.config.turns[0].outcome == AGENT_SESSION_TURN_ABORTED);
}

static void test_no_tool_storage(void)
{
    static unsigned char scratch[AGENT_SCRATCH_BYTES];
    static agent_session_ram_turn_t turns[1];
    static agent_session_ram_message_t stored_messages[2];
    static agent_message_view_t read_messages[2];
    static char payload[64];
    agent_session_ram_config_t config = {
        turns, 1u, stored_messages, 2u, NULL, 0u,
        payload, sizeof(payload), read_messages, 2u, NULL, 0u
    };
    agent_session_ram_t ram;
    agent_session_storage_t storage;
    agent_arena_t arena;
    agent_session_turn_t* turn;
    agent_message_view_t final = message(AGENT_MESSAGE_ROLE_ASSISTANT, SV("ok"));

    assert(agent_session_ram_init(&ram, &config) == AGENT_OK);
    assert(agent_session_ram_bind(&ram, &storage) == AGENT_OK);
    assert(agent_arena_init(&arena, scratch, sizeof(scratch)) == AGENT_OK);
    assert(agent_session_turn_open(&turn, &arena, &storage, SV("plain"), SV("hi")) == AGENT_OK);
    assert(agent_session_append(turn, &final) == AGENT_OK);
    assert(agent_session_turn_finish(turn, AGENT_SESSION_TURN_COMPLETE) == AGENT_OK);
    assert(storage.ops.clear(storage.context, SV("plain")) == AGENT_OK);
    assert(ram.turn_count == 0u);
}

int main(void)
{
    test_transactions_and_projection();
    test_capacity_and_missing_storage();
    test_no_tool_storage();
    return 0;
}
