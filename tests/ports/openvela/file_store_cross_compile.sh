#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
set -euo pipefail
root="$(cd "$(dirname "$0")/../../.." && pwd)"
: "${NUTTX_ROOT:?Set NUTTX_ROOT to the configured NuttX source tree}"
: "${CC:?Set CC to the target C compiler}"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
sources=(providers/storage/files/src/file_store.c
  ports/posix/storage/src/file_store.c
  providers/storage/jsonl/src/file_store_bind.c
  ports/openvela/storage/src/file_store.c)
index=0
for source in "${sources[@]}"; do
  "$CC" -std=c99 -Wall -Wextra -Werror ${CFLAGS:-} \
    -isystem "$NUTTX_ROOT/include" -I"$root/include" \
    -I"$root/providers/storage/files/include" -I"$root/providers/storage/jsonl/include" \
    -I"$root/ports/posix/storage/include" -I"$root/ports/openvela/storage/include" \
    -c "$root/$source" -o "$build/$index.o"
  index=$((index + 1))
done
printf 'PASS: file-store sources compile against configured NuttX headers (not firmware link)\n'
