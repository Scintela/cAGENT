/* SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200809L

#include <agent_espidf_session_files.h>

#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define SV(s) agent_string_view((s), sizeof(s) - 1u)

int main(void)
{
    char directory[] = "/tmp/cagent-espidf-storage-XXXXXX";
    char path[256];
    agent_espidf_session_files_t state = {0};
    agent_session_jsonl_config_t config = {0};
    size_t count = 0u;
    uint64_t bytes = 0u;

    assert(mkdtemp(directory) != NULL);
    assert(agent_port_espidf_session_files_init(NULL, directory, path,
                                                 sizeof(path), &config) == AGENT_ERROR_INVALID);
    assert(agent_port_espidf_session_files_init(&state, directory, path,
                                                 sizeof(path), NULL) == AGENT_ERROR_INVALID);
    assert(agent_port_espidf_session_files_init(&state, directory, path,
                                                 sizeof(path), &config) == AGENT_OK);
    assert(config.file_context == &state.files);
    assert(config.files.append(config.file_context, SV("home"), "one\n", 4u) == AGENT_OK);
    assert(config.files.sync(config.file_context, SV("home")) == AGENT_OK);
    assert(config.files.size(config.file_context, SV("home"), &bytes) == AGENT_OK &&
           bytes == 4u);
    assert(config.files.count(config.file_context, &count) == AGENT_OK && count == 1u);
    assert(config.files.clear_all(config.file_context) == AGENT_OK);
    assert(rmdir(directory) == 0);
    return 0;
}
