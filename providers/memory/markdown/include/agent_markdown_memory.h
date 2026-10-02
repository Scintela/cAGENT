/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* Optional whole-document Markdown Memory over application-authorized file stores. */
#pragma once

#include <agent/memory.h>
#include <agent_file_store.h>

#ifdef __cplusplus
extern "C" {
#endif

/* max_bytes=0 disables this document; store/name are borrowed, no file is opened at init. */
typedef struct {
    agent_file_store_t store;
    agent_string_view_t name;
    size_t max_bytes;
} agent_markdown_memory_document_t;

/* Identity/user roots come from trusted application configuration, never model arguments. */
typedef struct {
    agent_markdown_memory_document_t soul;
    agent_markdown_memory_document_t user;
    agent_markdown_memory_document_t facts;
    agent_file_store_t notes; /* Separate root from enabled documents; one flat daily-note namespace. */
    size_t note_max_bytes;   /* 0 disables notes; NOTE id is a trusted YYYY-MM-DD calendar date. */
} agent_markdown_memory_config_t;

/* No cache, heap or retained file descriptors; serialize shared stores and external edits. */
typedef struct {
    agent_markdown_memory_config_t config;
    bool active; /* Reentrant use is rejected; not an inter-thread lock. */
} agent_markdown_memory_t;

/* Validate enabled document mappings/capabilities and copy config, borrowing its references. */
agent_error_t agent_markdown_memory_init(agent_markdown_memory_t* memory,
                                         const agent_markdown_memory_config_t* config);

/* Export borrowed Memory ops; provider must outlive all bindings, reads return caller snapshots. */
agent_error_t agent_markdown_memory_bind(agent_markdown_memory_t* memory,
                                         agent_memory_t* binding);

#ifdef __cplusplus
}
#endif
