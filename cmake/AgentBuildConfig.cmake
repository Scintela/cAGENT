# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie

set(_AGENT_BUILD_CONFIG_TEMPLATE_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(agent_generate_build_config output_dir)
    set(_names
        MAX_TOOLS MAX_CONTEXTS MAX_SKILLS MAX_SESSIONS SESSION_EVENT_CAPACITY
        SESSION_PAYLOAD_BYTES MAX_PROJECTED_MESSAGES SCRATCH_BYTES MAX_INPUT_BYTES
        MAX_CONTEXT_BYTES MAX_SCHEMA_BYTES MAX_ARGUMENTS_BYTES MAX_TOOL_OUTPUT_BYTES
        MAX_MODEL_OUTPUT_BYTES MAX_MODEL_TOOL_CALLS MAX_NAME_BYTES MAX_DESCRIPTION_BYTES
        MAX_IDENTIFIER_BYTES MAX_JSON_DEPTH CORE_WORKSPACE_BYTES MODEL_WORKSPACE_BYTES
        DEFAULT_MAX_STEPS DEFAULT_TIMEOUT_MS DEFAULT_MODEL_TIMEOUT_MS DEFAULT_TOOL_TIMEOUT_MS
        DEFAULT_MAX_TOOL_CALLS DEFAULT_MAX_OUTPUT_TOKENS DEFAULT_MAX_HISTORY_TURNS)
    set(_defaults
        12 8 8 4 96
        8192 32 12288 1024
        4096 2048 1024 1024
        2048 4 64 256
        64 16 32768 128
        8 30000 15000 3000
        4 512 0)

    list(LENGTH _names _count)
    math(EXPR _last "${_count} - 1")
    foreach(_index RANGE ${_last})
        list(GET _names ${_index} _name)
        list(GET _defaults ${_index} _default)
        if(NOT DEFINED CONFIG_AGENT_${_name})
            set(CONFIG_AGENT_${_name} "${_default}" CACHE STRING
                "cAgent build-profile value for ${_name}")
        endif()
        set(AGENT_CFG_${_name} "${CONFIG_AGENT_${_name}}")
    endforeach()

    file(MAKE_DIRECTORY "${output_dir}")
    configure_file(
        "${_AGENT_BUILD_CONFIG_TEMPLATE_DIR}/agent_build_config.h.in"
        "${output_dir}/agent_build_config.h"
        @ONLY)
endfunction()
