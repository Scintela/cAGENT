# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie

if(CONFIG_AGENT_PORT_OPENVELA_RUNTIME OR CONFIG_AGENT_PORT_OPENVELA_TRANSPORT OR
   CONFIG_AGENT_PORT_OPENVELA_STORAGE)
    if(NOT COMMAND nuttx_add_library)
        message(FATAL_ERROR "OpenVela Port must be built in a NuttX application build")
    endif()
endif()

if(CONFIG_AGENT_PORT_OPENVELA_STORAGE)
    if(NOT CONFIG_AGENT_SESSION_JSONL)
        message(FATAL_ERROR "OpenVela Storage requires CONFIG_AGENT_SESSION_JSONL")
    endif()

    nuttx_add_library(cagent_openvela_storage STATIC)
    target_sources(cagent_openvela_storage PRIVATE
        storage/src/storage.c
        ${CMAKE_CURRENT_LIST_DIR}/../posix/storage/src/session_files.c)
    target_include_directories(cagent_openvela_storage
        PUBLIC
            ${CMAKE_CURRENT_LIST_DIR}/storage/include
            ${CMAKE_CURRENT_LIST_DIR}/../posix/storage/include
            ${CMAKE_CURRENT_LIST_DIR}/../../providers/storage/jsonl/include
            ${CMAKE_CURRENT_LIST_DIR}/../../include)
    target_compile_features(cagent_openvela_storage PUBLIC c_std_99)
endif()

if(CONFIG_AGENT_PORT_OPENVELA_RUNTIME)
    nuttx_add_library(cagent_openvela_runtime STATIC)
    target_sources(cagent_openvela_runtime PRIVATE runtime/src/runtime.c)
    target_include_directories(cagent_openvela_runtime
        PUBLIC
            ${CMAKE_CURRENT_LIST_DIR}/runtime/include
            ${CMAKE_CURRENT_LIST_DIR}/../../include)
    target_compile_features(cagent_openvela_runtime PUBLIC c_std_99)
endif()

if(CONFIG_AGENT_PORT_OPENVELA_TRANSPORT)
    if(NOT CONFIG_NETUTILS_WEBCLIENT)
        message(FATAL_ERROR "OpenVela Transport requires CONFIG_NETUTILS_WEBCLIENT")
    endif()

    nuttx_add_library(cagent_openvela_transport STATIC)
    target_sources(cagent_openvela_transport PRIVATE transport/src/transport.c)
    target_include_directories(cagent_openvela_transport
        PUBLIC
            ${CMAKE_CURRENT_LIST_DIR}/transport/include
            ${CMAKE_CURRENT_LIST_DIR}/../../include)
    target_compile_features(cagent_openvela_transport PUBLIC c_std_99)
endif()
