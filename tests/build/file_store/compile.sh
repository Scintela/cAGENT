#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
set -euo pipefail
root="$(cd "$(dirname "$0")/../../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
for mode in posix idf openvela; do
  for variant in off on; do
    cmake -S "$root/tests/build/file_store" -B "$build/$mode-$variant" \
      -DFILE_STORE_PLATFORM="$mode" -DFILE_STORE_WITH_JSONL="$variant" \
      -DFILE_STORE_WITH_MEMORY="$variant" \
      -DFILE_STORE_REUSE="$variant" -DCMAKE_C_FLAGS='-Wall -Wextra -Werror' \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > "$build/configure.log"
    cmake --build "$build/$mode-$variant" -j4 > "$build/build.log"
    "$build/$mode-$variant/file_store_smoke"
    commands="$build/$mode-$variant/compile_commands.json"
    if [[ "$variant" == off ]]; then
      if rg -q '/codecs/json/|/providers/storage/jsonl/|/providers/memory/' "$commands"; then
        printf 'FAIL: file-only build unexpectedly includes JSON/JSONL/Markdown Memory\n' >&2
        exit 1
      fi
    fi
    # Both shared implementations must be compiled exactly once per assembly.
    for source in providers/storage/files/src/file_store.c ports/posix/storage/src/file_store.c; do
      count="$(rg -c '"file": ".*'"$source"'"' "$commands")"
      test "$count" = 1
    done
    if [[ "$variant" == on ]]; then
      count="$(rg -c '"file": ".*/providers/memory/markdown/src/memory_markdown.c"' "$commands")"
      test "$count" = 1
    fi
    printf 'PASS: %s file-store assembly (%s)\n' "$mode" "$variant"
  done
done
if cmake -S "$root" -B "$build/memory-without-files" \
    -DAGENT_BUILD_MARKDOWN_MEMORY=ON -DAGENT_BUILD_FILE_STORE=OFF > "$build/missing.log" 2>&1; then
  printf 'FAIL: Markdown Memory accepted missing File Store dependency\n' >&2
  exit 1
fi
rg -q 'requires AGENT_BUILD_FILE_STORE' "$build/missing.log"
cmake -S "$root/tests/build/file_store" -B "$build/memory-no-json" \
  -DFILE_STORE_WITH_MEMORY=ON -DCMAKE_C_FLAGS='-Wall -Wextra -Werror' \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > "$build/configure.log"
cmake --build "$build/memory-no-json" -j4 > "$build/build.log"
"$build/memory-no-json/file_store_smoke"
if rg -q '/codecs/json/|/providers/storage/jsonl/' "$build/memory-no-json/compile_commands.json"; then
  printf 'FAIL: Markdown Memory unexpectedly depends on JSON/JSONL\n' >&2
  exit 1
fi
printf 'PASS: Markdown Memory dependency validation and JSON-free assembly\n'
