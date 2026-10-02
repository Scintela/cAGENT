/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 tanglinjie */
#include <agent_file_store.h>
#include <assert.h>
#include <string.h>

#define SV(s) agent_string_view((s), sizeof(s) - 1u)
typedef struct {
    const char* data;
    size_t length;
    size_t chunk;
    unsigned int size_calls;
    int mutation;
    bool invalid_count;
    bool published;
} mock_t;

static agent_error_t mock_size(void* ctx, agent_string_view_t name, uint64_t* bytes)
{
    mock_t* mock = ctx;
    (void)name;
    *bytes = mock->length + (mock->mutation == 2 && mock->size_calls > 0u ? 1u : 0u);
    ++mock->size_calls;
    return AGENT_OK;
}

static agent_error_t mock_read(void* ctx, agent_string_view_t name, uint64_t offset,
                                void* output, size_t capacity, size_t* count)
{
    mock_t* mock = ctx;
    size_t available;
    (void)name;
    if (mock->invalid_count) { *count = capacity + 1u; return AGENT_OK; }
    if (mock->mutation == 1 && offset == mock->length && capacity) {
        *(char*)output = 'x'; *count = 1u; return AGENT_OK;
    }
    if (mock->mutation == -1 && offset >= mock->length - 1u) { *count = 0u; return AGENT_OK; }
    available = offset < mock->length ? mock->length - (size_t)offset : 0u;
    *count = available < capacity ? available : capacity;
    if (*count > mock->chunk) *count = mock->chunk;
    if (*count) memcpy(output, mock->data + (size_t)offset, *count);
    if (mock->mutation == 3 && offset == 0u && *count) mock->data = "world";
    return AGENT_OK;
}

static agent_error_t mock_visit(void* ctx, agent_file_visit_fn visitor, void* user)
{
    bool stop = false;
    (void)ctx;
    /* Intentionally ignore errors/stop: wrapper must keep the first result sticky. */
    (void)visitor(user, SV("USER.md"), &stop);
    (void)visitor(user, SV("MEMORY.md"), &stop);
    return AGENT_OK;
}

static agent_error_t visitor(void* ctx, agent_string_view_t name, bool* stop)
{
    unsigned int* calls = ctx;
    assert(name.size > 0u);
    ++*calls;
    *stop = true;
    return AGENT_ERROR_CANCELLED;
}

static agent_error_t mock_replace(void* ctx, agent_string_view_t name,
                                   const void* data, size_t bytes, bool* published)
{
    mock_t* mock = ctx;
    (void)name; (void)data; (void)bytes;
    *published = mock->published;
    return AGENT_ERROR_IO;
}

int main(void)
{
    const agent_file_store_ops_t ops = {mock_size, mock_read, mock_visit,
        NULL, NULL, NULL, NULL, mock_replace};
    const agent_file_store_ops_t empty_ops = {0};
    mock_t mock = {"hello", 5u, 1u, 0u, 0, false, false};
    agent_file_store_t store = {&ops, &mock};
    agent_file_store_t empty = {&empty_ops, NULL};
    agent_string_view_t text;
    char output[8];
    size_t count = 99u;
    uint64_t bytes;
    unsigned int calls = 0u;
    bool published = true;
    assert(agent_file_read_exact(&store, SV("USER.md"), 0u, output, 5u) == AGENT_OK);
    assert(memcmp(output, "hello", 5u) == 0);
    assert(agent_file_read_exact(&store, SV("USER.md"), 3u, output, 3u) == AGENT_ERROR_IO);
    assert(agent_file_read(&store, SV("USER.md"), UINT64_MAX, output, 2u, &count) == AGENT_ERROR_INVALID);
    assert(count == 0u);
    assert(agent_file_read_text(&store, SV("USER.md"), output, sizeof(output), 5u, &text) == AGENT_OK);
    assert(text.size == 5u && strcmp(output, "hello") == 0);
    assert(agent_file_read_text(&store, SV("USER.md"), output, 5u, 5u, &text) == AGENT_ERROR_CAPACITY);
    assert(!text.data && output[0] == '\0');
    assert(agent_file_read_all(&store, SV("USER.md"), output, sizeof(output), 4u, &count) == AGENT_ERROR_CAPACITY);
    assert(count == 0u);
    mock.data = "a\0b"; mock.length = 3u;
    assert(agent_file_read_text(&store, SV("USER.md"), output, sizeof(output), 5u, &text) == AGENT_ERROR_PARSE);
    mock.length = 0u;
    assert(agent_file_read_text(&store, SV("USER.md"), output, 1u, 5u, &text) == AGENT_OK);
    assert(text.size == 0u && output[0] == '\0');
    mock.data = "hello"; mock.length = 5u;
    for (mock.mutation = -1; mock.mutation <= 2; ++mock.mutation) {
        mock.size_calls = 0u;
        assert(agent_file_read_all(&store, SV("USER.md"), output, sizeof(output), 8u, &count) ==
               (mock.mutation == 0 ? AGENT_OK : AGENT_ERROR_IO));
        assert(count == (mock.mutation == 0 ? 5u : 0u));
    }
    /* Deliberately violate stable-input preconditions: size checks cannot detect this. */
    mock.mutation = 3; mock.size_calls = 0u;
    assert(agent_file_read_all(&store, SV("USER.md"), output, sizeof(output), 8u, &count) == AGENT_OK);
    assert(count == 5u && memcmp(output, "horld", 5u) == 0);
    mock.data = "hello"; mock.mutation = 0; mock.invalid_count = true;
    assert(agent_file_read(&store, SV("USER.md"), 0u, output, 1u, &count) == AGENT_ERROR_IO);
    assert(count == 0u);
    assert(agent_file_size(&store, SV("../USER.md"), &bytes) == AGENT_ERROR_INVALID);
    assert(agent_file_size(&store, SV("."), &bytes) == AGENT_ERROR_INVALID);
    assert(agent_file_size(&store, SV(".."), &bytes) == AGENT_ERROR_INVALID);
    assert(agent_file_size(&store, SV("a\\b"), &bytes) == AGENT_ERROR_INVALID);
    assert(agent_file_size(&store, SV("a\0b"), &bytes) == AGENT_ERROR_INVALID);
    assert(agent_file_size(&store, SV(".cagent-private"), &bytes) == AGENT_ERROR_INVALID);
    assert(agent_file_read(&empty, SV("USER.md"), 0u, output, 1u, &count) == AGENT_ERROR_NOT_SUPPORTED);
    assert(agent_file_append(&store, SV("USER.md"), "x", 1u) == AGENT_ERROR_NOT_SUPPORTED);
    assert(agent_file_truncate(&store, SV("USER.md"), 0u) == AGENT_ERROR_NOT_SUPPORTED);
    assert(agent_file_sync(&store, SV("USER.md")) == AGENT_ERROR_NOT_SUPPORTED);
    assert(agent_file_remove(&store, SV("USER.md")) == AGENT_ERROR_NOT_SUPPORTED);
    assert(agent_file_visit(&store, visitor, &calls) == AGENT_ERROR_CANCELLED && calls == 1u);
    assert(agent_file_replace(&store, SV("USER.md"), "x", 1u, &published) == AGENT_ERROR_IO && !published);
    mock.published = true;
    assert(agent_file_replace(&store, SV("USER.md"), "x", 1u, &published) == AGENT_ERROR_IO && published);
    assert(agent_file_replace(&empty, SV("USER.md"), "x", 1u, &published) == AGENT_ERROR_NOT_SUPPORTED && !published);
    return 0;
}
