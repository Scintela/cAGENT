/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 tanglinjie */
#define _POSIX_C_SOURCE 200809L
#include <agent_posix_file_store.h>
#ifdef TEST_JSONL
#include <agent_session_jsonl_files.h>
#endif
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define SV(s) agent_string_view((s), sizeof(s) - 1u)

int main(void)
{
    char root[] = "/tmp/cagent-file-build-XXXXXX", paths[512], output[32];
    agent_posix_file_store_t state;
    agent_posix_file_store_config_t config = {root, paths, sizeof(paths), false, false};
    agent_file_store_t store;
    agent_string_view_t text;
    assert(mkdtemp(root));
    assert(agent_posix_file_store_init(&state, &config, &store) == AGENT_OK);
    assert(agent_file_append(&store, SV("USER.md"), "user", 4u) == AGENT_OK);
    assert(agent_file_read_text(&store, SV("USER.md"), output, sizeof(output), 31u, &text) == AGENT_OK);
    assert(!strcmp(output, "user"));
#ifdef TEST_JSONL
    {
        agent_session_jsonl_files_t files;
        agent_session_jsonl_config_t jsonl_config = {0};
        char names[160];
        uint64_t length;
        assert(agent_session_jsonl_files_init(&files, &store, names, sizeof(names), &jsonl_config) == AGENT_OK);
        assert(jsonl_config.files.append(&files, SV("chat"), "test", 4u) == AGENT_OK);
        assert(agent_file_size(&store, SV("session-63686174.jsonl"), &length) == AGENT_OK && length == 4u);
        assert(jsonl_config.files.clear_all(&files) == AGENT_OK);
        assert(agent_file_read_text(&store, SV("USER.md"), output, sizeof(output), 31u, &text) == AGENT_OK);
    }
#endif
    assert(agent_file_remove(&store, SV("USER.md")) == AGENT_OK);
    assert(rmdir(root) == 0);
    return 0;
}
