/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* POSIX-backed JSONL Session files in an application-owned directory. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <agent_posix_session_files.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define FILE_PREFIX "session-"
#define FILE_SUFFIX ".jsonl"

static agent_error_t io_error(void)
{
    return errno == ENOENT ? AGENT_ERROR_NOT_FOUND : AGENT_ERROR_IO;
}

static bool ready(const agent_posix_session_files_t* files)
{
    return files && files->directory && files->directory_length &&
           files->path && files->path_capacity;
}

static bool overlaps(const void* left, size_t left_size,
                     const void* right, size_t right_size)
{
    uintptr_t a = (uintptr_t)left;
    uintptr_t b = (uintptr_t)right;

    if (left_size > UINTPTR_MAX - a || right_size > UINTPTR_MAX - b) return true;
    return a < b + right_size && b < a + left_size;
}

static size_t base_length(const agent_posix_session_files_t* files)
{
    return files->directory_length +
           (files->directory[files->directory_length - 1u] == '/' ? 0u : 1u);
}

static void put_base(agent_posix_session_files_t* files)
{
    memcpy(files->path, files->directory, files->directory_length);
    if (base_length(files) != files->directory_length)
        files->path[files->directory_length] = '/';
}

static agent_error_t session_path(agent_posix_session_files_t* files,
                                  agent_string_view_t id)
{
    static const char hex[] = "0123456789abcdef";
    size_t base;
    size_t remaining;
    size_t i;
    size_t pos;

    if (!ready(files) || !id.data || !id.size ||
        overlaps(id.data, id.size, files->path, files->path_capacity))
        return AGENT_ERROR_INVALID;
    base = base_length(files);
    if (base >= files->path_capacity) return AGENT_ERROR_CAPACITY;
    remaining = files->path_capacity - base;
    if (remaining <= sizeof(FILE_PREFIX) - 1u + sizeof(FILE_SUFFIX) ||
        id.size > (remaining - (sizeof(FILE_PREFIX) - 1u) -
                   sizeof(FILE_SUFFIX)) / 2u)
        return AGENT_ERROR_CAPACITY;
    put_base(files);
    pos = base;
    memcpy(files->path + pos, FILE_PREFIX, sizeof(FILE_PREFIX) - 1u);
    pos += sizeof(FILE_PREFIX) - 1u;
    for (i = 0u; i < id.size; ++i) {
        unsigned char byte = (unsigned char)id.data[i];
        files->path[pos++] = hex[byte >> 4u];
        files->path[pos++] = hex[byte & 15u];
    }
    memcpy(files->path + pos, FILE_SUFFIX, sizeof(FILE_SUFFIX));
    return AGENT_OK;
}

static bool session_entry(const char* name)
{
    size_t length = strlen(name);
    size_t start = sizeof(FILE_PREFIX) - 1u;
    size_t suffix = sizeof(FILE_SUFFIX) - 1u;
    size_t i;

    if (length < start + 2u + suffix ||
        (length - start - suffix) % 2u ||
        memcmp(name, FILE_PREFIX, start) != 0 ||
        strcmp(name + length - suffix, FILE_SUFFIX) != 0)
        return false;
    for (i = start; i < length - suffix; ++i) {
        if (!((name[i] >= '0' && name[i] <= '9') ||
              (name[i] >= 'a' && name[i] <= 'f')))
            return false;
    }
    return true;
}

static agent_error_t entry_path(agent_posix_session_files_t* files, const char* name)
{
    size_t base = base_length(files);
    size_t length = strlen(name);

    if (base >= files->path_capacity || length >= files->path_capacity - base)
        return AGENT_ERROR_CAPACITY;
    put_base(files);
    memcpy(files->path + base, name, length + 1u);
    return AGENT_OK;
}

agent_error_t agent_posix_session_files_init(agent_posix_session_files_t* files,
                                              const char* directory,
                                              char* path_buffer, size_t path_capacity)
{
    struct stat info;
    size_t length;
    size_t base;

    if (!files || !directory || !path_buffer || !path_capacity) return AGENT_ERROR_INVALID;
    length = strlen(directory);
    if (!length || directory[0] != '/' || length == SIZE_MAX ||
        overlaps(directory, length + 1u, path_buffer, path_capacity))
        return AGENT_ERROR_INVALID;
    if (stat(directory, &info) != 0) return io_error();
    if (!S_ISDIR(info.st_mode)) return AGENT_ERROR_INVALID;
    base = length + (directory[length - 1u] == '/' ? 0u : 1u);
    if (base < length || base >= path_capacity ||
        path_capacity - base < sizeof(FILE_PREFIX) - 1u + 2u + sizeof(FILE_SUFFIX))
        return AGENT_ERROR_CAPACITY;
    files->directory = directory;
    files->directory_length = length;
    files->path = path_buffer;
    files->path_capacity = path_capacity;
    return AGENT_OK;
}

static agent_error_t file_size(void* context, agent_string_view_t id, uint64_t* bytes)
{
    agent_posix_session_files_t* files = context;
    struct stat info;
    agent_error_t status;

    if (!bytes) return AGENT_ERROR_INVALID;
    status = session_path(files, id);
    if (status != AGENT_OK) return status;
    if (stat(files->path, &info) != 0) return io_error();
    if (!S_ISREG(info.st_mode) || info.st_size < 0) return AGENT_ERROR_IO;
    *bytes = (uint64_t)info.st_size;
    return AGENT_OK;
}

static agent_error_t file_read(void* context, agent_string_view_t id, uint64_t offset,
                                char* output, size_t size)
{
    agent_posix_session_files_t* files = context;
    off_t position = (off_t)offset;
    agent_error_t status;
    size_t done = 0u;
    int fd;

    if ((size && !output) || position < 0 || (uint64_t)position != offset ||
        offset > UINT64_MAX - size)
        return AGENT_ERROR_INVALID;
    status = session_path(files, id);
    if (status != AGENT_OK) return status;
    fd = open(files->path, O_RDONLY);
    if (fd < 0) return io_error();
    {
        off_t actual;
        do {
            actual = lseek(fd, position, SEEK_SET);
        } while (actual < 0 && errno == EINTR);
        if (actual != position) status = AGENT_ERROR_IO;
    }
    while (status == AGENT_OK && done < size) {
        size_t amount = size - done > (size_t)SSIZE_MAX ? (size_t)SSIZE_MAX : size - done;
        ssize_t count = read(fd, output + done, amount);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) status = AGENT_ERROR_IO;
        else done += (size_t)count;
    }
    if (close(fd) != 0 && status == AGENT_OK) status = AGENT_ERROR_IO;
    return status;
}

static agent_error_t file_append(void* context, agent_string_view_t id,
                                  const char* data, size_t size)
{
    agent_posix_session_files_t* files = context;
    agent_error_t status;
    size_t done = 0u;
    int fd;

    if (size && !data) return AGENT_ERROR_INVALID;
    status = session_path(files, id);
    if (status != AGENT_OK) return status;
    fd = open(files->path, O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (fd < 0) return io_error();
    while (done < size) {
        size_t amount = size - done > (size_t)SSIZE_MAX ? (size_t)SSIZE_MAX : size - done;
        ssize_t count = write(fd, data + done, amount);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
            status = AGENT_ERROR_IO;
            break;
        }
        done += (size_t)count;
    }
    if (close(fd) != 0 && status == AGENT_OK) status = AGENT_ERROR_IO;
    return status;
}

static agent_error_t file_truncate(void* context, agent_string_view_t id, uint64_t size)
{
    agent_posix_session_files_t* files = context;
    off_t length = (off_t)size;
    agent_error_t status;
    int fd;

    if (length < 0 || (uint64_t)length != size) return AGENT_ERROR_INVALID;
    status = session_path(files, id);
    if (status != AGENT_OK) return status;
    fd = open(files->path, O_WRONLY);
    if (fd < 0) return io_error();
    if (ftruncate(fd, length) != 0) status = AGENT_ERROR_IO;
    if (close(fd) != 0 && status == AGENT_OK) status = AGENT_ERROR_IO;
    return status;
}

static agent_error_t file_sync(void* context, agent_string_view_t id)
{
    agent_posix_session_files_t* files = context;
    agent_error_t status;
    int fd;

    status = session_path(files, id);
    if (status != AGENT_OK) return status;
    fd = open(files->path, O_RDONLY);
    if (fd < 0) return io_error();
    while (fsync(fd) != 0) {
        if (errno != EINTR) {
            status = AGENT_ERROR_IO;
            break;
        }
    }
    if (close(fd) != 0 && status == AGENT_OK) status = AGENT_ERROR_IO;
    return status;
}

static agent_error_t file_remove(void* context, agent_string_view_t id)
{
    agent_posix_session_files_t* files = context;
    agent_error_t status = session_path(files, id);

    if (status != AGENT_OK) return status;
    return unlink(files->path) == 0 ? AGENT_OK : io_error();
}

static agent_error_t walk(agent_posix_session_files_t* files, bool remove_entries,
                          size_t* count)
{
    DIR* dir;
    struct dirent* entry;
    agent_error_t status = AGENT_OK;
    size_t found = 0u;

    if (!ready(files)) return AGENT_ERROR_INVALID;
    dir = opendir(files->directory);
    if (!dir) return io_error();
    for (;;) {
        struct stat info;

        errno = 0;
        entry = readdir(dir);
        if (!entry) {
            if (errno != 0) status = AGENT_ERROR_IO;
            break;
        }
        if (!session_entry(entry->d_name)) continue;
        status = entry_path(files, entry->d_name);
        if (status != AGENT_OK) break;
        if (stat(files->path, &info) != 0) {
            status = io_error();
            break;
        }
        if (!S_ISREG(info.st_mode)) continue;
        if (found == SIZE_MAX) {
            status = AGENT_ERROR_CAPACITY;
            break;
        }
        ++found;
        if (remove_entries && unlink(files->path) != 0) {
            status = io_error();
            break;
        }
    }
    if (closedir(dir) != 0 && status == AGENT_OK) status = AGENT_ERROR_IO;
    if (status == AGENT_OK && count) *count = found;
    return status;
}

static agent_error_t file_clear_all(void* context)
{
    return walk(context, true, NULL);
}

static agent_error_t file_count(void* context, size_t* count)
{
    if (!count) return AGENT_ERROR_INVALID;
    return walk(context, false, count);
}

agent_session_jsonl_file_ops_t agent_posix_session_file_ops(void)
{
    agent_session_jsonl_file_ops_t ops = {
        file_size, file_read, file_append, file_truncate,
        file_sync, file_remove, file_clear_all, file_count
    };

    return ops;
}
