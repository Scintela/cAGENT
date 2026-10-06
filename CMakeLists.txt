# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie

cmake_minimum_required(VERSION 3.16)

set(AGENT_CORE_SOURCES
    src/context/context_builder.c
    src/context/context_registry.c
    src/core/lifecycle.c
    src/core/event.c
    src/core/arena.c
    src/core/error.c
    src/memory/memory_mgr.c
    src/model/model.c
    src/policy/policy_chain.c
    src/run/react_loop.c
    src/run/cancel.c
    src/runtime/runtime.c
    src/session/session_manager.c
    src/session/session_storage.c
    src/skill/skill_registry.c
    src/tool/tool_guard.c
    src/tool/tool_registry.c
    src/tool/tool_schema.c
    src/transport/transport.c)

if(COMMAND idf_component_register)
    include("${CMAKE_CURRENT_LIST_DIR}/cmake/AgentBuildConfig.cmake")
    agent_generate_build_config("${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(_agent_sources ${AGENT_CORE_SOURCES})
    set(_agent_public_includes "include" "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(_agent_private_includes "src")
    if(CONFIG_AGENT_PROVIDER_OPENAI)
        list(APPEND _agent_sources
            providers/model/openai/src/model_openai.c
            providers/model/openai/src/openai_request.c
            providers/model/openai/src/openai_response.c)
        list(APPEND _agent_public_includes providers/model/openai/include)
        list(APPEND _agent_private_includes providers/model/openai/src)
    endif()
    if(CONFIG_AGENT_SESSION_RAM)
        list(APPEND _agent_sources providers/storage/ram/src/session_ram.c)
        list(APPEND _agent_public_includes providers/storage/ram/include)
    endif()
    if(CONFIG_AGENT_FILE_STORE)
        list(APPEND _agent_sources providers/storage/files/src/file_store.c)
        list(APPEND _agent_public_includes providers/storage/files/include)
    endif()
    if(CONFIG_AGENT_MEMORY_MARKDOWN)
        if(NOT CONFIG_AGENT_FILE_STORE)
            message(FATAL_ERROR "Markdown Memory requires CONFIG_AGENT_FILE_STORE")
        endif()
        list(APPEND _agent_sources providers/memory/markdown/src/memory_markdown.c)
        list(APPEND _agent_public_includes providers/memory/markdown/include)
    endif()
    if(CONFIG_AGENT_POSIX_FILE_STORE)
        if(NOT CONFIG_AGENT_FILE_STORE)
            message(FATAL_ERROR "POSIX file store requires CONFIG_AGENT_FILE_STORE")
        endif()
        list(APPEND _agent_sources ports/posix/storage/src/file_store.c)
        list(APPEND _agent_public_includes ports/posix/storage/include)
        list(APPEND _agent_private_requires vfs)
    endif()
    if(CONFIG_AGENT_SESSION_JSONL)
        list(APPEND _agent_sources
            providers/storage/jsonl/src/session_jsonl.c
            providers/storage/jsonl/src/jsonl_record.c)
        list(APPEND _agent_public_includes providers/storage/jsonl/include)
        list(APPEND _agent_private_includes providers/storage/jsonl/src)
        if(CONFIG_AGENT_FILE_STORE)
            list(APPEND _agent_sources providers/storage/jsonl/src/file_store_bind.c)
        endif()
    endif()
    if(CONFIG_AGENT_MAX_TOOLS GREATER 0 OR CONFIG_AGENT_PROVIDER_OPENAI OR CONFIG_AGENT_SESSION_JSONL)
        list(APPEND _agent_sources codecs/json/reader.c)
        list(APPEND _agent_private_includes codecs/json codecs/json/vendor/jsmn)
    endif()
    if(CONFIG_AGENT_PROVIDER_OPENAI OR CONFIG_AGENT_SESSION_JSONL)
        list(APPEND _agent_sources codecs/json/writer.c)
    endif()
    if(_agent_private_requires)
        idf_component_register(
            SRCS ${_agent_sources}
            INCLUDE_DIRS ${_agent_public_includes}
            PRIV_INCLUDE_DIRS ${_agent_private_includes}
            PRIV_REQUIRES ${_agent_private_requires})
    else()
        idf_component_register(
            SRCS ${_agent_sources}
            INCLUDE_DIRS ${_agent_public_includes}
            PRIV_INCLUDE_DIRS ${_agent_private_includes})
    endif()
    target_compile_definitions(${COMPONENT_LIB} PUBLIC
        AGENT_BUILD_CONFIG_HEADER="agent_build_config.h")
    target_compile_features(${COMPONENT_LIB} PUBLIC c_std_99)
    if(CONFIG_AGENT_POSIX_FILE_STORE)
        target_compile_definitions(${COMPONENT_LIB} PRIVATE
            AGENT_POSIX_FILE_STORE_NO_SYMLINKS=1)
    endif()
    return()
endif()

project(cagent VERSION 0.2.0 LANGUAGES C)

option(AGENT_BUILD_MOCK_PROVIDER "Build the mock Model provider" OFF)
option(AGENT_BUILD_OPENAI_PROVIDER "Build the OpenAI-compatible Model provider" OFF)
option(AGENT_BUILD_JSON_CODEC "Build the private bounded jsmn JSON codec" OFF)
option(AGENT_BUILD_SESSION_RAM "Build the volatile caller-storage Session provider" OFF)
option(AGENT_BUILD_SESSION_JSONL "Build the file-ops-injected JSONL Session provider" OFF)
option(AGENT_BUILD_FILE_STORE "Build optional byte-file contracts and bounded helpers" OFF)
option(AGENT_BUILD_MARKDOWN_MEMORY "Build the bounded file-backed Markdown Memory provider" OFF)
option(AGENT_BUILD_POSIX_FILE_STORE "Build the optional POSIX byte-file backend" OFF)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/AgentBuildConfig.cmake")
agent_generate_build_config("${CMAKE_CURRENT_BINARY_DIR}/generated/include")

add_library(cagent_core STATIC ${AGENT_CORE_SOURCES})
add_library(cagent::core ALIAS cagent_core)
target_include_directories(cagent_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(cagent_core PUBLIC
    AGENT_BUILD_CONFIG_HEADER="agent_build_config.h")
target_compile_features(cagent_core PUBLIC c_std_99)
set_target_properties(cagent_core PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)

if(AGENT_BUILD_JSON_CODEC OR CONFIG_AGENT_MAX_TOOLS GREATER 0)
    add_subdirectory(codecs/json)
endif()
if(CONFIG_AGENT_MAX_TOOLS GREATER 0)
    target_include_directories(cagent_core PRIVATE codecs/json codecs/json/vendor/jsmn)
    target_link_libraries(cagent_core PRIVATE cagent_json_reader)
endif()

if(AGENT_BUILD_MOCK_PROVIDER)
    add_subdirectory(providers/model/mock)
endif()

if(AGENT_BUILD_OPENAI_PROVIDER)
    add_subdirectory(providers/model/openai)
endif()

if(AGENT_BUILD_SESSION_RAM)
    add_subdirectory(providers/storage/ram)
endif()

if(AGENT_BUILD_FILE_STORE)
    add_subdirectory(providers/storage/files)
endif()
if(AGENT_BUILD_MARKDOWN_MEMORY)
    add_subdirectory(providers/memory/markdown)
endif()

if(AGENT_BUILD_SESSION_JSONL)
    add_subdirectory(providers/storage/jsonl)
endif()

if(AGENT_BUILD_POSIX_FILE_STORE)
    add_subdirectory(ports/posix/storage)
endif()
