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
      -DFILE_STORE_REUSE="$variant" -DCMAKE_C_FLAGS='-Wall -Wextra -Werror' \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > "$build/configure.log"
    cmake --build "$build/$mode-$variant" -j4 > "$build/build.log"
    "$build/$mode-$variant/file_store_smoke"
    commands="$build/$mode-$variant/compile_commands.json"
    if [[ "$variant" == off ]]; then
      if rg -q '/codecs/json/|/providers/storage/jsonl/' "$commands"; then
        printf 'FAIL: file-only build unexpectedly includes JSON/JSONL\n' >&2
        exit 1
      fi
    fi
    # Both shared implementations must be compiled exactly once per assembly.
    for source in providers/storage/files/src/file_store.c ports/posix/storage/src/file_store.c; do
      count="$(rg -c '"file": ".*'"$source"'"' "$commands")"
      test "$count" = 1
    done
    printf 'PASS: %s file-store assembly (%s)\n' "$mode" "$variant"
  done
done
