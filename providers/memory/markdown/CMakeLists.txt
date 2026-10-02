# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
if(NOT TARGET cagent_file_store)
    message(FATAL_ERROR "Markdown Memory requires AGENT_BUILD_FILE_STORE=ON")
endif()
add_library(cagent_memory_markdown STATIC src/memory_markdown.c)
add_library(cagent::memory_markdown ALIAS cagent_memory_markdown)
target_include_directories(cagent_memory_markdown PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(cagent_memory_markdown PUBLIC cagent_file_store)
target_compile_features(cagent_memory_markdown PUBLIC c_std_99)
set_target_properties(cagent_memory_markdown PROPERTIES
    C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
