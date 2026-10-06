#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
set -euo pipefail
root="$(cd "$(dirname "$0")/../../.." && pwd)"
binary="$(mktemp)"
trap 'rm -f "$binary"' EXIT
includes=(-I"$root/include" -I"$root/src" -I"$root/providers/storage/files/include"
  -I"$root/codecs/json" -I"$root/codecs/json/vendor/jsmn"
  -I"$root/providers/memory/markdown/include" -I"$root/ports/posix/storage/include")
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -pedantic ${CFLAGS:-} "${includes[@]}" \
  "$root/src/tool/tool_registry.c" "$root/src/tool/tool_schema.c" "$root/codecs/json/reader.c" \
  "$root/tests/providers/memory_markdown/contract.c" \
  "$root/providers/memory/markdown/src/memory_markdown.c" \
  "$root/providers/storage/files/src/file_store.c" "$root/ports/posix/storage/src/file_store.c" \
  "$root/src/memory/memory_mgr.c" "$root/src/core/lifecycle.c" "$root/src/core/arena.c" \
  "$root/src/model/model.c" "$root/src/runtime/runtime.c" -o "$binary"
"$binary"
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror "${includes[@]}" \
  -include agent_markdown_memory.h -x c++ -fsyntax-only /dev/null
printf 'PASS: Markdown Memory, Soul protection, user isolation, dates and mutation failures\n'
