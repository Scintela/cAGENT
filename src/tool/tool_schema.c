/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* Syntax admission only; application validators own JSON Schema semantics. */

#include "tool/tool_internal.h"

#if AGENT_MAX_TOOLS > 0
#include "json_internal.h"
#endif

agent_error_t agent_tool_text_validate(agent_string_view_t text)
{
    if (text.size && !text.data)
        return AGENT_ERROR_INVALID;
#if AGENT_MAX_TOOLS > 0
    {
        size_t pos = 0u;
        while (pos < text.size)
        {
            size_t width;
            agent_error_t status;
            if (text.data[pos] == '\0')
                return AGENT_ERROR_INVALID;
            status = agent_json_utf8_width((const unsigned char*)text.data + pos, text.size - pos,
                                           &width);
            if (status != AGENT_OK)
                return status;
            pos += width;
        }
    }
    return AGENT_OK;
#else
    return text.size ? AGENT_ERROR_NOT_SUPPORTED : AGENT_OK;
#endif
}

agent_error_t agent_tool_object_validate(agent_string_view_t object, size_t maximum)
{
    if (object.size > maximum)
        return AGENT_ERROR_LIMIT;
    if (!object.data || !object.size)
        return AGENT_ERROR_INVALID;
#if AGENT_MAX_TOOLS > 0
    return agent_json_validate_unique_object(object, AGENT_MAX_JSON_DEPTH);
#else
    return AGENT_ERROR_NOT_SUPPORTED;
#endif
}
