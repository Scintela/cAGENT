/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 tanglinjie */
/* Host linker wrapping verifies error outcomes without production fault hooks. */
#define _POSIX_C_SOURCE 200809L
#include <agent_posix_file_store.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SV(s) agent_string_view((s), sizeof(s) - 1u)
int __real_open(const char*, int, ...);
int __real_close(int);
ssize_t __real_read(int, void*, size_t);
ssize_t __real_write(int, const void*, size_t);
int __real_fsync(int);
int __real_rename(const char*, const char*);
int __real_closedir(DIR*);

static int handles;
static int sync_failure; /* 1: file error, 2: directory error, 3: unsupported sync */
static int write_failure;
static bool interrupt_read, interrupt_write, fail_rename, fail_close, fail_closedir;

int __wrap_open(const char* path, int flags, ...)
{
    int fd;
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = (mode_t)va_arg(args, int);
        va_end(args);
    }
    fd = __real_open(path, flags, mode);
    if (fd >= 0) ++handles;
    return fd;
}

int __wrap_close(int fd)
{
    int result = __real_close(fd);
    --handles;
    if (fail_close) { fail_close = false; errno = EIO; return -1; }
    return result;
}

ssize_t __wrap_read(int fd, void* output, size_t bytes)
{
    if (interrupt_read) { interrupt_read = false; errno = EINTR; return -1; }
    return __real_read(fd, output, bytes > 2u ? 2u : bytes);
}

ssize_t __wrap_write(int fd, const void* input, size_t bytes)
{
    if (interrupt_write) { interrupt_write = false; errno = EINTR; return -1; }
    if (write_failure > 0 && --write_failure == 0) { errno = ENOSPC; return -1; }
    return __real_write(fd, input, bytes > 2u ? 2u : bytes);
}

int __wrap_fsync(int fd)
{
    struct stat info;
    assert(fstat(fd, &info) == 0);
    if (sync_failure == 3) { errno = ENOSYS; return -1; }
    if ((sync_failure == 1 && S_ISREG(info.st_mode)) ||
        (sync_failure == 2 && S_ISDIR(info.st_mode))) { errno = EIO; return -1; }
    return __real_fsync(fd);
}

int __wrap_rename(const char* from, const char* to)
{
    if (fail_rename) { errno = EIO; return -1; }
    return __real_rename(from, to);
}

int __wrap_closedir(DIR* directory)
{
    int result = __real_closedir(directory);
    if (fail_closedir) { fail_closedir = false; errno = EIO; return -1; }
    return result;
}

static agent_error_t count_names(void* ctx, agent_string_view_t name, bool* stop)
{
    size_t* count = ctx;
    (void)stop;
    assert(name.size == 7u && !memcmp(name.data, "USER.md", 7u));
    ++*count;
    return AGENT_OK;
}

static void expect_text(agent_file_store_t* store, const char* expected)
{
    char text[32];
    agent_string_view_t view;
    assert(agent_file_read_text(store, SV("USER.md"), text, sizeof(text), 31u, &view) == AGENT_OK);
    assert(strcmp(text, expected) == 0 && handles == 0);
}

int main(void)
{
    char root[] = "/tmp/cagent-files-faults-XXXXXX", scratch[512], output[32];
    agent_posix_file_store_t state;
    agent_file_store_t store;
    agent_posix_file_store_config_t config = {root, scratch, sizeof(scratch), false, true};
    bool published;
    size_t count;
    assert(mkdtemp(root));
    sync_failure = 2;
    assert(agent_posix_file_store_init(&state, &config, &store) == AGENT_ERROR_IO && handles == 0);
    sync_failure = 0;
    assert(agent_posix_file_store_init(&state, &config, &store) == AGENT_OK && handles == 0);
    interrupt_write = true;
    assert(agent_file_append(&store, SV("USER.md"), "old", 3u) == AGENT_OK && handles == 0);
    interrupt_read = true;
    expect_text(&store, "old");
    sync_failure = 1;
    assert(agent_file_replace(&store, SV("USER.md"), "new", 3u, &published) == AGENT_ERROR_IO && !published);
    sync_failure = 0;
    expect_text(&store, "old");
    sync_failure = 3;
    assert(agent_file_replace(&store, SV("USER.md"), "new", 3u, &published) == AGENT_ERROR_NOT_SUPPORTED && !published);
    sync_failure = 0;
    expect_text(&store, "old");
    fail_rename = true;
    assert(agent_file_replace(&store, SV("USER.md"), "new", 3u, &published) == AGENT_ERROR_IO && !published);
    fail_rename = false;
    expect_text(&store, "old");
    fail_close = true;
    assert(agent_file_replace(&store, SV("USER.md"), "new", 3u, &published) == AGENT_ERROR_IO && !published);
    expect_text(&store, "old");
    write_failure = 2;
    assert(agent_file_replace(&store, SV("USER.md"), "changed", 7u, &published) == AGENT_ERROR_IO && !published);
    expect_text(&store, "old");
    sync_failure = 2;
    assert(agent_file_replace(&store, SV("USER.md"), "new", 3u, &published) == AGENT_ERROR_IO && published);
    sync_failure = 0;
    expect_text(&store, "new");
    write_failure = 2;
    assert(agent_file_append(&store, SV("USER.md"), "suffix", 6u) == AGENT_ERROR_IO);
    expect_text(&store, "newsu");
    fail_close = true;
    count = 99u;
    assert(agent_file_read(&store, SV("USER.md"), 0u, output, sizeof(output), &count) == AGENT_ERROR_IO && count == 0u);
    assert(handles == 0);
    count = 0u;
    fail_closedir = true;
    assert(agent_file_visit(&store, count_names, &count) == AGENT_ERROR_IO && count == 1u);
    count = 0u;
    assert(agent_file_visit(&store, count_names, &count) == AGENT_OK && count == 1u);
    assert(agent_file_remove(&store, SV("USER.md")) == AGENT_OK && handles == 0);
    assert(rmdir(root) == 0); /* No hidden temporary files leaked by failed replacement. */
    return 0;
}
