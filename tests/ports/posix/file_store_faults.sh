#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
set -euo pipefail
root="$(cd "$(dirname "$0")/../../.." && pwd)"
binary="$(mktemp)"
trap 'rm -f "$binary"' EXIT
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -pedantic ${CFLAGS:-} \
  -I"$root/include" -I"$root/providers/storage/files/include" \
  -I"$root/ports/posix/storage/include" \
  "$root/tests/ports/posix/file_store_faults.c" \
  "$root/providers/storage/files/src/file_store.c" \
  "$root/ports/posix/storage/src/file_store.c" \
  -Wl,--wrap=open,--wrap=close,--wrap=read,--wrap=write,--wrap=fsync,--wrap=rename,--wrap=closedir \
  -o "$binary"
"$binary"
printf 'PASS: POSIX short I/O, failed replacement and resource cleanup (GNU linker)\n'
