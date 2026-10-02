/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* Explicit document selection and whole-document publication; no Markdown interpretation. */
#include <agent_markdown_memory.h>

#include <stdint.h>
#include <string.h>

static bool overlaps(const void* a, size_t na, const void* b, size_t nb)
{
    uintptr_t left = (uintptr_t)a, right = (uintptr_t)b;
    if (!na || !nb) return false;
    if (na > UINTPTR_MAX - left || nb > UINTPTR_MAX - right) return true;
    return left < right + nb && right < left + na;
}

static bool same_store(const agent_file_store_t* a, const agent_file_store_t* b)
{
    return a->ops == b->ops && a->context == b->context;
}

static bool same_name(agent_string_view_t a, agent_string_view_t b)
{
    return a.size == b.size && (!a.size || !memcmp(a.data, b.data, a.size));
}

static bool valid_name(agent_string_view_t name)
{
    size_t i;
    if (!name.data || !name.size ||
        (name.size == 1u && name.data[0] == '.') ||
        (name.size == 2u && !memcmp(name.data, "..", 2u)) ||
        (name.size >= 8u && !memcmp(name.data, ".cagent-", 8u))) return false;
    for (i = 0u; i < name.size; ++i)
        if (!name.data[i] || name.data[i] == '/' || name.data[i] == '\\') return false;
    return true;
}

static unsigned int date_number(const char* text, size_t digits)
{
    unsigned int value = 0u;
    size_t i;
    for (i = 0u; i < digits; ++i) value = value * 10u + (unsigned int)(text[i] - '0');
    return value;
}

static bool valid_date(agent_string_view_t id)
{
    static const unsigned int days[] = {31u, 28u, 31u, 30u, 31u, 30u,
                                        31u, 31u, 30u, 31u, 30u, 31u};
    unsigned int year, month, day, limit;
    size_t i;
    if (!id.data || id.size != 10u || id.data[4] != '-' || id.data[7] != '-') return false;
    for (i = 0u; i < id.size; ++i) {
        if (i != 4u && i != 7u && (id.data[i] < '0' || id.data[i] > '9')) return false;
    }
    year = date_number(id.data, 4u);
    month = date_number(id.data + 5u, 2u);
    day = date_number(id.data + 8u, 2u);
    if (!year || month < 1u || month > 12u || !day) return false;
    limit = days[month - 1u];
    if (month == 2u && (!(year % 400u) || (!(year % 4u) && year % 100u))) ++limit;
    return day <= limit;
}

static agent_error_t select_document(agent_markdown_memory_t* memory,
                                     const agent_memory_key_t* key, char note_name[15],
                                     agent_markdown_memory_document_t* document)
{
    if (!key) return AGENT_ERROR_INVALID;
    switch (key->kind) {
    case AGENT_MEMORY_SOUL: *document = memory->config.soul; break;
    case AGENT_MEMORY_USER: *document = memory->config.user; break;
    case AGENT_MEMORY_FACTS: *document = memory->config.facts; break;
    case AGENT_MEMORY_NOTE:
        if (!valid_date(key->id)) return AGENT_ERROR_INVALID;
        memcpy(note_name, key->id.data, 10u);
        memcpy(note_name + 10u, ".md", 4u);
        document->store = memory->config.notes;
        document->name = agent_string_view(note_name, 13u);
        document->max_bytes = memory->config.note_max_bytes;
        break;
    default: return AGENT_ERROR_INVALID;
    }
    if (key->kind != AGENT_MEMORY_NOTE && key->id.size) return AGENT_ERROR_INVALID;
    return document->max_bytes ? AGENT_OK : AGENT_ERROR_NOT_SUPPORTED;
}

static bool safe_buffer(const agent_markdown_memory_t* memory, const void* data, size_t bytes)
{
    const agent_markdown_memory_document_t* docs[] = {
        &memory->config.soul, &memory->config.user, &memory->config.facts
    };
    size_t i;
    if (overlaps(data, bytes, memory, sizeof(*memory))) return false;
    for (i = 0u; i < 3u; ++i) {
        if (overlaps(data, bytes, docs[i]->name.data, docs[i]->name.size)) return false;
    }
    return true;
}

static agent_error_t markdown_read(void* context, const agent_memory_key_t* key,
                                   char* output, size_t capacity, size_t max_bytes, size_t* bytes)
{
    agent_markdown_memory_t* memory = context;
    agent_markdown_memory_document_t document;
    agent_string_view_t text;
    char note_name[15];
    agent_error_t status;
    if (!memory || !bytes || !output || !capacity || !max_bytes ||
        !safe_buffer(memory, output, capacity) || !safe_buffer(memory, bytes, sizeof(*bytes)) ||
        overlaps(output, capacity, bytes, sizeof(*bytes)) ||
        overlaps(bytes, sizeof(*bytes), key, key ? sizeof(*key) : 0u) ||
        overlaps(output, capacity, key, key ? sizeof(*key) : 0u) ||
        (key && (overlaps(output, capacity, key->id.data, key->id.size) ||
                 overlaps(bytes, sizeof(*bytes), key->id.data, key->id.size))))
        return AGENT_ERROR_INVALID;
    *bytes = 0u;
    if (memory->active) return AGENT_ERROR_BUSY;
    status = select_document(memory, key, note_name, &document);
    if (status != AGENT_OK) return status;
    if (max_bytes > document.max_bytes) max_bytes = document.max_bytes;
    memory->active = true;
    status = agent_file_read_text(&document.store, document.name, output, capacity,
                                  max_bytes, &text);
    memory->active = false;
    if (status == AGENT_OK) *bytes = text.size;
    return status;
}

static agent_error_t markdown_mutate(void* context, const agent_memory_key_t* key,
                                     agent_string_view_t text, bool forget,
                                     agent_memory_change_t* change)
{
    agent_markdown_memory_t* memory = context;
    agent_markdown_memory_document_t document;
    char note_name[15];
    bool published = false;
    agent_error_t status;
    if (!memory || !change || !safe_buffer(memory, change, sizeof(*change)) ||
        overlaps(change, sizeof(*change), key, key ? sizeof(*key) : 0u) ||
        overlaps(change, sizeof(*change), text.data, text.size) ||
        (key && overlaps(change, sizeof(*change), key->id.data, key->id.size)))
        return AGENT_ERROR_INVALID;
    *change = AGENT_MEMORY_UNCHANGED;
    if (memory->active) return AGENT_ERROR_BUSY;
    status = select_document(memory, key, note_name, &document);
    if (status != AGENT_OK) return status;
    if (key->kind == AGENT_MEMORY_SOUL) return AGENT_ERROR_POLICY_DENIED;
    if (!forget && ((text.size && !text.data) || !safe_buffer(memory, text.data, text.size)))
        return AGENT_ERROR_INVALID;
    if (!forget && text.size > document.max_bytes) return AGENT_ERROR_CAPACITY;
    if (!forget && text.size && memchr(text.data, '\0', text.size)) return AGENT_ERROR_PARSE;
    memory->active = true;
    if (forget) {
        if (!document.store.ops->remove) status = AGENT_ERROR_NOT_SUPPORTED;
        else {
            status = agent_file_remove(&document.store, document.name);
            if (status == AGENT_OK) *change = AGENT_MEMORY_APPLIED;
            else if (status != AGENT_ERROR_NOT_FOUND) *change = AGENT_MEMORY_UNKNOWN;
        }
    } else {
        status = agent_file_replace(&document.store, document.name, text.data, text.size, &published);
        if (published) *change = AGENT_MEMORY_APPLIED;
    }
    memory->active = false;
    return status;
}

static agent_error_t markdown_replace(void* context, const agent_memory_key_t* key,
                                      agent_string_view_t text, agent_memory_change_t* change)
{
    return markdown_mutate(context, key, text, false, change);
}

static agent_error_t markdown_forget(void* context, const agent_memory_key_t* key,
                                     agent_memory_change_t* change)
{
    return markdown_mutate(context, key, agent_string_view(NULL, 0u), true, change);
}

agent_error_t agent_markdown_memory_init(agent_markdown_memory_t* memory,
                                         const agent_markdown_memory_config_t* config)
{
    const agent_markdown_memory_document_t* docs[3];
    size_t i, j;
    bool enabled = false;
    if (!memory || !config || overlaps(memory, sizeof(*memory), config, sizeof(*config)))
        return AGENT_ERROR_INVALID;
    docs[0] = &config->soul; docs[1] = &config->user; docs[2] = &config->facts;
    for (i = 0u; i < 3u; ++i) {
        if (!docs[i]->max_bytes) continue;
        enabled = true;
        if (!valid_name(docs[i]->name) ||
            overlaps(memory, sizeof(*memory), docs[i]->name.data, docs[i]->name.size))
            return AGENT_ERROR_INVALID;
        if (!docs[i]->store.ops || !docs[i]->store.ops->read || !docs[i]->store.ops->size)
            return AGENT_ERROR_NOT_SUPPORTED;
        for (j = 0u; j < i; ++j) {
            if (docs[j]->max_bytes && same_store(&docs[i]->store, &docs[j]->store) &&
                same_name(docs[i]->name, docs[j]->name)) return AGENT_ERROR_INVALID;
        }
        if (config->note_max_bytes && same_store(&docs[i]->store, &config->notes))
            return AGENT_ERROR_INVALID;
    }
    if (config->note_max_bytes) {
        enabled = true;
        if (!config->notes.ops || !config->notes.ops->read || !config->notes.ops->size)
            return AGENT_ERROR_NOT_SUPPORTED;
    }
    if (!enabled) return AGENT_ERROR_INVALID;
    memory->config = *config;
    memory->active = false;
    return AGENT_OK;
}

agent_error_t agent_markdown_memory_bind(agent_markdown_memory_t* memory,
                                         agent_memory_t* binding)
{
    const agent_memory_ops_t ops = {markdown_read, markdown_replace, markdown_forget};
    if (!memory || !binding || !safe_buffer(memory, binding, sizeof(*binding)))
        return AGENT_ERROR_INVALID;
    if (memory->active) return AGENT_ERROR_BUSY;
    if (!memory->config.soul.max_bytes && !memory->config.user.max_bytes &&
        !memory->config.facts.max_bytes && !memory->config.note_max_bytes) return AGENT_ERROR_STATE;
    binding->ops = ops;
    binding->context = memory;
    return AGENT_OK;
}
