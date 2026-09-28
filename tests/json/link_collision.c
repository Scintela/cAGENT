/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* Simulate an application that also compiles its own globally linked jsmn. */
#include "jsmn.h"
#include "json_internal.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    static const char input[] = "{\"x\":1}";
    jsmn_parser parser;
    jsmntok_t application_tokens[4];
    jsmntok_t codec_tokens[4];
    agent_json_document_t document;

    jsmn_init(&parser);
    assert(jsmn_parse(&parser, input, sizeof(input) - 1u,
                      application_tokens, 4u) == 3);
    assert(agent_json_parse(agent_string_view(input, sizeof(input) - 1u),
                            codec_tokens, 4u, 4u, &document) == AGENT_OK);
    assert(document.token_count == 3u);
    puts("PASS: codec links alongside application jsmn");
    return 0;
}
