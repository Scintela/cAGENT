# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie

if(CONFIG_AGENT_PORT_OPENVELA_RUNTIME OR CONFIG_AGENT_PORT_OPENVELA_TRANSPORT OR
   CONFIG_AGENT_PORT_OPENVELA_STORAGE OR CONFIG_AGENT_PORT_OPENVELA_FILE_STORE)
    if(NOT COMMAND nuttx_add_library)
        message(FATAL_ERROR "OpenVela Port must be built in a NuttX application build")
    endif()
endif()

if(CONFIG_AGENT_PORT_OPENVELA_FILE_STORE)
    if(NOT CONFIG_AGENT_FILE_STORE)
        message(FATAL_ERROR "OpenVela file store requires CONFIG_AGENT_FILE_STORE")
    endif()
    # Reuse existing targets when the application already builds these packages.
    if(NOT TARGET cagent_file_store)
        nuttx_add_library(cagent_file_store STATIC)
        target_sources(cagent_file_store PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/../../providers/storage/files/src/file_store.c)
        target_include_directories(cagent_file_store PUBLIC
            ${CMAKE_CURRENT_LIST_DIR}/../../providers/storage/files/include
            ${CMAKE_CURRENT_LIST_DIR}/../../include)
        target_compile_features(cagent_file_store PUBLIC c_std_99)
    endif()
    if(NOT TARGET cagent_posix_file_store)
        nuttx_add_library(cagent_posix_file_store STATIC)
        target_sources(cagent_posix_file_store PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/../posix/storage/src/file_store.c)
        target_include_directories(cagent_posix_file_store PUBLIC
            ${CMAKE_CURRENT_LIST_DIR}/../posix/storage/include)
        target_link_libraries(cagent_posix_file_store PUBLIC cagent_file_store)
        target_compile_features(cagent_posix_file_store PUBLIC c_std_99)
    endif()
    nuttx_add_library(cagent_openvela_file_store STATIC)
    target_sources(cagent_openvela_file_store PRIVATE storage/src/file_store.c)
    target_include_directories(cagent_openvela_file_store PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/storage/include)
    target_link_libraries(cagent_openvela_file_store PUBLIC cagent_posix_file_store)
    target_compile_features(cagent_openvela_file_store PUBLIC c_std_99)
endif()

if(CONFIG_AGENT_PORT_OPENVELA_STORAGE)
    if(NOT CONFIG_AGENT_SESSION_JSONL)
        message(FATAL_ERROR "OpenVela Storage requires CONFIG_AGENT_SESSION_JSONL")
    endif()

    if(NOT TARGET cagent_posix_session_files)
        nuttx_add_library(cagent_posix_session_files STATIC)
        target_sources(cagent_posix_session_files PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/../posix/storage/src/session_files.c)
        target_include_directories(cagent_posix_session_files PUBLIC
            ${CMAKE_CURRENT_LIST_DIR}/../posix/storage/include
            ${CMAKE_CURRENT_LIST_DIR}/../../providers/storage/jsonl/include
            ${CMAKE_CURRENT_LIST_DIR}/../../include)
        target_compile_features(cagent_posix_session_files PUBLIC c_std_99)
    endif()
    nuttx_add_library(cagent_openvela_storage STATIC)
    target_sources(cagent_openvela_storage PRIVATE storage/src/storage.c)
    target_include_directories(cagent_openvela_storage PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/storage/include)
    target_link_libraries(cagent_openvela_storage PUBLIC cagent_posix_session_files)
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
