/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
#include "json_internal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define SV(literal) agent_string_view((literal), sizeof(literal) - 1u)

static agent_error_t parse_text(agent_string_view_t input, size_t token_capacity,
                                size_t max_depth, agent_json_document_t* document,
                                jsmntok_t* tokens)
{
    return agent_json_parse(input, tokens, token_capacity, max_depth, document);
}

static void test_valid_values(void)
{
    static const char* const values[] = {
        "null", "true", "false", "0", "-12.5e+2", "\"hello\"", "[]", "{}",
        "[1,{\"nested\":true},null]", "{\"key\":\"\\uD83D\\uDE00\"}",
        "\"\xe4\xb8\xad\""
    };
    size_t i;
    for (i = 0u; i < sizeof(values) / sizeof(values[0]); ++i) {
        agent_json_document_t document;
        jsmntok_t tokens[48];
        agent_string_view_t value = agent_string_view(values[i], strlen(values[i]));
        assert(parse_text(value, 48u, 16u, &document, tokens) == AGENT_OK);
        assert(document.token_count > 0u);
    }
}

static void test_invalid_values(void)
{
    static const char* const values[] = {
        "", "tru", "False", "01", "-", "1.", "1e", "+1", "NaN", "{} trailing",
        "{\"a\":1,}", "{\"a\" 1}", "{\"a\":1 \"b\":2}", "[1,]", "[1 2]",
        "\"bad\nline\"", "\"\\x\"", "\"\\uD800\"", "\"\\uDC00\"",
        "\"\\uD800\\u0041\"", "\"\\u123\"", "\"unterminated"
    };
    size_t i;
    for (i = 0u; i < sizeof(values) / sizeof(values[0]); ++i) {
        agent_json_document_t document;
        jsmntok_t tokens[48];
        agent_string_view_t value = agent_string_view(values[i], strlen(values[i]));
        agent_error_t status = parse_text(value, 48u, 16u, &document, tokens);
        assert(status != AGENT_OK);
        assert(document.token_count == 0u);
    }
    {
        static const char embedded_nul[] = {'[', '0', ']', '\0', '[', '1', ']'};
        static const char bad_utf8[] = {'"', (char)0xc0, (char)0xaf, '"'};
        agent_json_document_t document;
        jsmntok_t tokens[8];
        assert(parse_text(agent_string_view(embedded_nul, sizeof(embedded_nul)),
                          8u, 16u, &document, tokens) == AGENT_ERROR_PARSE);
        assert(parse_text(agent_string_view(bad_utf8, sizeof(bad_utf8)),
                          8u, 16u, &document, tokens) == AGENT_ERROR_PARSE);
    }
}

static void test_limits(void)
{
    agent_json_document_t document;
    jsmntok_t tokens[8];

    assert(parse_text(SV("[[0]]"), 8u, 1u, &document, tokens) == AGENT_ERROR_LIMIT);
    assert(parse_text(SV("[[0]]"), 8u, 2u, &document, tokens) == AGENT_OK);
    assert(parse_text(SV("[1,2,3]"), 2u, 16u, &document, tokens) ==
           AGENT_ERROR_CAPACITY);
    assert(parse_text(SV("null"), 8u, 33u, &document, tokens) ==
           AGENT_ERROR_INVALID);
}

static void test_object_validation(void)
{
    assert(agent_json_validate_object(SV(" {\"on\":true} \n"), 16u) == AGENT_OK);
    assert(agent_json_validate_object(SV("[1]"), 16u) == AGENT_ERROR_PARSE);
    assert(agent_json_validate_object(SV("{\"on\":true} trailing"), 16u) ==
           AGENT_ERROR_PARSE);
    assert(agent_json_validate_object(SV("{\"nested\":{}}"), 1u) ==
           AGENT_ERROR_LIMIT);
    assert(agent_json_validate_object(SV("{}"), 0u) == AGENT_ERROR_INVALID);
}

static void test_paths_and_decoding(void)
{
    agent_json_document_t document;
    jsmntok_t tokens[32];
    size_t value;
    char decoded_bytes[16];
    agent_string_view_t decoded;
    agent_string_view_t raw;
    agent_json_path_segment_t path[3] = {
        {AGENT_JSON_PATH_KEY, SV("items"), 0u},
        {AGENT_JSON_PATH_INDEX, {NULL, 0u}, 0u},
        {AGENT_JSON_PATH_KEY, SV("name"), 0u}
    };

    assert(parse_text(SV("{\"items\":[{\"na\\u006de\":\"a\\uD83D\\uDE00\"},2]}"),
                      32u, 16u, &document, tokens) == AGENT_OK);
    assert(agent_json_path_get(&document, 0u, path, 3u, &value) == AGENT_OK);
    assert(agent_json_decode_string(&document, value, decoded_bytes,
                                    sizeof(decoded_bytes), &decoded) == AGENT_OK);
    assert(decoded.size == 5u);
    assert(memcmp(decoded.data, "a\xf0\x9f\x98\x80", 5u) == 0);
    assert(agent_json_raw_value(&document, value, &raw) == AGENT_OK);
    assert(raw.size == sizeof("\"a\\uD83D\\uDE00\"") - 1u);
    assert(memcmp(raw.data, "\"a\\uD83D\\uDE00\"", raw.size) == 0);
    assert(agent_json_decode_string(&document, value, decoded_bytes, 4u, &decoded) ==
           AGENT_ERROR_CAPACITY);
    assert(agent_json_object_get(&document, 0u, SV("missing"), &value) ==
           AGENT_ERROR_NOT_FOUND);
    assert(agent_json_array_get(&document, 2u, 5u, &value) ==
           AGENT_ERROR_NOT_FOUND);

    assert(parse_text(SV("{\"name\":1,\"na\\u006de\":2}"), 32u, 16u,
                      &document, tokens) == AGENT_OK);
    assert(agent_json_object_get(&document, 0u, SV("name"), &value) ==
           AGENT_ERROR_PARSE);
}

static void test_writer(void)
{
    agent_json_writer_t writer;
    agent_string_view_t output;
    agent_json_document_t document;
    jsmntok_t tokens[32];
    char buffer[128];

    assert(parse_text(SV("{\"enabled\":true}"), 32u, 16u, &document, tokens) ==
           AGENT_OK);
    agent_json_writer_init(&writer, buffer, sizeof(buffer));
    assert(agent_json_writer_literal(&writer, SV("{\"text\":")) == AGENT_OK);
    assert(agent_json_writer_string(&writer, SV("a\"\\\n")) == AGENT_OK);
    assert(agent_json_writer_literal(&writer, SV(",\"count\":")) == AGENT_OK);
    assert(agent_json_writer_u32(&writer, UINT32_MAX) == AGENT_OK);
    assert(agent_json_writer_literal(&writer, SV(",\"flag\":")) == AGENT_OK);
    assert(agent_json_writer_bool(&writer, false) == AGENT_OK);
    assert(agent_json_writer_literal(&writer, SV(",\"schema\":")) == AGENT_OK);
    assert(agent_json_writer_raw_value(&writer, &document, 0u) == AGENT_OK);
    assert(agent_json_writer_literal(&writer, SV("}")) == AGENT_OK);
    assert(agent_json_writer_finish(&writer, &output) == AGENT_OK);
    assert(output.size == strlen(output.data));
    assert(strstr(output.data, "\"text\":\"a\\\"\\\\\\u000a\"") != NULL);
    assert(strstr(output.data, "4294967295") != NULL);
    assert(parse_text(output, 32u, 16u, &document, tokens) == AGENT_OK);

    agent_json_writer_init(&writer, buffer, 3u);
    assert(agent_json_writer_string(&writer, SV("abc")) == AGENT_ERROR_CAPACITY);
    assert(agent_json_writer_finish(&writer, &output) == AGENT_ERROR_CAPACITY);
    assert(output.data == NULL);

    {
        static const char invalid[] = {(char)0xc0, (char)0xaf};
        agent_json_writer_init(&writer, buffer, sizeof(buffer));
        assert(agent_json_writer_string(&writer,
                                        agent_string_view(invalid, sizeof(invalid))) ==
               AGENT_ERROR_PARSE);
        assert(agent_json_writer_finish(&writer, &output) == AGENT_ERROR_PARSE);
    }

    agent_json_writer_init(&writer, buffer, sizeof(buffer));
    assert(agent_json_writer_string(&writer, agent_string_view(NULL, 1u)) ==
           AGENT_ERROR_INVALID);
    assert(agent_json_writer_finish(&writer, &output) == AGENT_ERROR_INVALID);

    agent_json_writer_init(&writer, buffer, sizeof(buffer));
    assert(agent_json_writer_raw_value(&writer, &document, 100u) ==
           AGENT_ERROR_INVALID);
    assert(agent_json_writer_finish(&writer, &output) == AGENT_ERROR_INVALID);
}

static void test_random_smoke(void)
{
    uint32_t state = 0x1a2b3c4du;
    size_t trial;

    for (trial = 0u; trial < 10000u; ++trial) {
        char input[48];
        jsmntok_t tokens[64];
        agent_json_document_t document;
        size_t size;
        size_t i;

        state = state * 1664525u + 1013904223u;
        size = state % sizeof(input);
        for (i = 0u; i < size; ++i) {
            state = state * 1664525u + 1013904223u;
            input[i] = (char)(state >> 24);
        }
        if (agent_json_parse(agent_string_view(input, size), tokens, 64u, 8u,
                             &document) == AGENT_OK) {
            agent_string_view_t raw;
            assert(document.token_count > 0u);
            assert(agent_json_raw_value(&document, 0u, &raw) == AGENT_OK);
            assert(raw.size <= size);
        }
    }
}

int main(void)
{
    test_valid_values();
    test_invalid_values();
    test_limits();
    test_object_validation();
    test_paths_and_decoding();
    test_writer();
    test_random_smoke();
    puts("PASS: private bounded jsmn JSON codec");
    return 0;
}
