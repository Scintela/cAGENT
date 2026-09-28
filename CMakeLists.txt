# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie

cmake_minimum_required(VERSION 3.16)

set(AGENT_CORE_SOURCES
    src/context/context_builder.c
    src/context/context_registry.c
    src/core/agent_core.c
    src/core/agent_event.c
    src/core/arena.c
    src/core/error.c
    src/memory/memory_mgr.c
    src/model/model.c
    src/policy/policy_chain.c
    src/run/react_loop.c
    src/run/cancel.c
    src/runtime/runtime.c
    src/session/session_codec.c
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
    idf_component_register(
        SRCS ${AGENT_CORE_SOURCES}
        INCLUDE_DIRS "include" "${CMAKE_CURRENT_BINARY_DIR}/generated"
        PRIV_INCLUDE_DIRS "src")
    target_compile_definitions(${COMPONENT_LIB} PUBLIC
        AGENT_BUILD_CONFIG_HEADER="agent_build_config.h")
    target_compile_features(${COMPONENT_LIB} PUBLIC c_std_99)
    return()
endif()

project(cagent VERSION 0.2.0 LANGUAGES C)

option(AGENT_BUILD_MOCK_PROVIDER "Build the mock Model provider" OFF)
option(AGENT_BUILD_OPENAI_PROVIDER "Build the OpenAI-compatible Model provider" OFF)

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

if(AGENT_BUILD_MOCK_PROVIDER)
    add_subdirectory(providers/mock)
endif()

if(AGENT_BUILD_OPENAI_PROVIDER)
    add_subdirectory(providers/openai)
endif()
