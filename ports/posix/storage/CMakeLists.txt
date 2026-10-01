# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie

if(NOT TARGET cagent_session_jsonl)
    message(FATAL_ERROR "POSIX Session files require AGENT_BUILD_SESSION_JSONL=ON")
endif()

add_library(cagent_posix_session_files STATIC src/session_files.c)
add_library(cagent::posix_session_files ALIAS cagent_posix_session_files)
target_include_directories(cagent_posix_session_files
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(cagent_posix_session_files PUBLIC cagent_session_jsonl)
target_compile_features(cagent_posix_session_files PUBLIC c_std_99)
set_target_properties(cagent_posix_session_files PROPERTIES
    C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
