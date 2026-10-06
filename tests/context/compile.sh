#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
sources=("$root/src/context/context_registry.c" "$root/src/context/context_builder.c"
  "$root/src/context/context_projection.c" "$root/src/skill/skill_registry.c"
  "$root/src/tool/tool_registry.c" "$root/src/tool/tool_schema.c"
  "$root/src/core/lifecycle.c" "$root/src/core/arena.c" "$root/src/model/model.c"
  "$root/src/memory/memory_mgr.c" "$root/src/session/session_manager.c"
  "$root/src/session/session_storage.c" "$root/providers/storage/ram/src/session_ram.c"
  "$root/src/runtime/runtime.c" "$root/src/run/cancel.c")
for slots in 0 1 8; do
  "${CC:-cc}" -std=c99 -Wall -Wextra -Werror -pedantic ${CFLAGS:-} \
    -I"$root/include" -I"$root/src" -I"$root/providers/storage/ram/include" \
    -I"$root/codecs/json" -I"$root/codecs/json/vendor/jsmn" \
    -DAGENT_MAX_CONTEXTS="$slots" "$root/tests/context/contract.c" \
    "${sources[@]}" "$root/codecs/json/reader.c" -o "$build/context-$slots"
  "$build/context-$slots"
done
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -pedantic ${CFLAGS:-} \
  -I"$root/include" -I"$root/src" -I"$root/providers/storage/ram/include" \
  -DAGENT_MAX_TOOLS=0 -DAGENT_MAX_SKILLS=0 \
  "$root/tests/context/contract.c" "${sources[@]}" -o "$build/no-json"
"$build/no-json"
printf 'PASS: Context projection, snapshots, rollback, cancellation and trimmed profiles\n'
