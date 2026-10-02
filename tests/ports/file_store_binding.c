/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 tanglinjie */
#define _POSIX_C_SOURCE 200809L
#if defined(TEST_ESPIDF)
#include <agent_espidf_file_store.h>
#define file_state_t agent_espidf_file_store_t
#define file_config_t agent_espidf_file_store_config_t
#define init_store agent_port_espidf_file_store_init
#elif defined(TEST_OPENVELA)
#include <agent_openvela_file_store.h>
#define file_state_t agent_openvela_file_store_t
#define file_config_t agent_openvela_file_store_config_t
#define init_store agent_port_openvela_file_store_init
#else
#error Select the platform binding to test
#endif
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define SV(s) agent_string_view((s), sizeof(s) - 1u)

int main(void)
{
    char root[] = "/tmp/cagent-platform-files-XXXXXX", paths[512], output[32];
    file_state_t state;
    file_config_t config = {root, paths, sizeof(paths), false, false};
    agent_file_store_t store;
    agent_string_view_t text;
    bool published;
    assert(mkdtemp(root));
    assert(init_store(NULL, &config, &store) == AGENT_ERROR_INVALID);
    assert(init_store(&state, &config, &store) == AGENT_OK);
    assert(agent_file_append(&store, SV("USER.md"), "user", 4u) == AGENT_OK);
    assert(agent_file_sync(&store, SV("USER.md")) == AGENT_OK);
    assert(agent_file_read_text(&store, SV("USER.md"), output, sizeof(output), 31u, &text) == AGENT_OK);
    assert(text.size == 4u && !strcmp(output, "user"));
    assert(agent_file_replace(&store, SV("USER.md"), "updated", 7u, &published) == AGENT_OK && published);
    assert(agent_file_remove(&store, SV("USER.md")) == AGENT_OK);
    config.read_only = true;
    assert(init_store(&state, &config, &store) == AGENT_OK);
    assert(agent_file_append(&store, SV("USER.md"), "x", 1u) == AGENT_ERROR_NOT_SUPPORTED);
    assert(rmdir(root) == 0);
    return 0;
}
