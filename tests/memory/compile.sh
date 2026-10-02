#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
binary="$(mktemp)"
trap 'rm -f "$binary"' EXIT
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -pedantic ${CFLAGS:-} \
  -I"$root/include" -I"$root/src" \
  "$root/tests/memory/contract.c" "$root/src/memory/memory_mgr.c" \
  "$root/src/core/arena.c" "$root/src/core/lifecycle.c" \
  "$root/src/model/model.c" "$root/src/runtime/runtime.c" -o "$binary"
"$binary"
printf 'PASS: Memory binding, read/write bounds, outcomes, reentry and turn snapshots\n'
